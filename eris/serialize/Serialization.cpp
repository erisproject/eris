#include <eris/serialize/Serialization.hpp>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <boost/filesystem/operations.hpp>

namespace eris { namespace serialize {

namespace fs = boost::filesystem;

constexpr unsigned int Serialization::POINTER_BLOCK_SIZE;

char eris_magic[4] = {'e', 'r', 'i', 's'};
char xz_magic[6] = {char(0xfd), '7', 'z', 'X', 'Z', char(0)};

Serialization::Serialization() {}


void Serialization::memory() {

    close();

    f_.reset(new std::stringstream(open_readwrite));

    read_only_ = false;
    compress_ = false;
    changed_ = false;
    memory_only_ = true;

    writeHeader();
}

void Serialization::open(
        const std::string &filename,
        Mode mode,
        bool compress_new,
        bool memory,
        const std::string &tmpdir
        ) {

    close();

    // These will be updated below, if necessary.
    read_only_ = false;
    changed_ = false;
    compress_ = false;
    memory_only_ = false;

    try {
        auto *f = new std::fstream;
        f_.reset(f);
        f->exceptions(f->failbit | f->badbit);

        // Whether the file is permitted to be empty:
        bool allow_empty = true;

        switch (mode) {
            case Mode::READONLY:
                f->open(filename, open_readonly);
                read_only_ = true;
                allow_empty = false;
                break;
            case Mode::APPEND:
                f->open(filename, open_readwrite);
                allow_empty = false;
                break;
            case Mode::READWRITE:
                // If the file exists, we need to open with open_readwrite; otherwise, we open with
                // open_overwrite: this is because open_readwrite will fail if the file doesn't
                // exist (unless also truncating, but we don't want to do that with this Mode).
                f->open(filename, fs::exists(filename) ? open_readwrite : open_overwrite);
                // An empty file is fine here (even if it already existed, but was empty):
                allow_empty = true;
                break;
            case Mode::OVERWRITE:
                f->open(filename, open_overwrite);
                allow_empty = true;
                break;
        }

        f->seekg(0, f->end);
        auto file_size = f->tellg();
        if (file_size == 0) {
            if (not allow_empty)
                throw parse_error("File to read is empty");

            if (compress_new) {
                compress_ = true;

                // We have an empty file which should be compressed when finished.  That means we
                // need either an in-memory buffer or an intermediate temp file; we'll save the
                // opened, empty file as final_f_ to be written to during close/destroy.
                final_f_.swap(f_);

                if (memory) {
                    f_.reset(new std::stringstream(open_readwrite));
                }
                else {
                    auto tempfile = Serialization::tempfile(filename, tmpdir);

                    auto *tmpf = new std::fstream;
                    f_.reset(tmpf);
                    tmpf->exceptions(tmpf->failbit | tmpf->badbit);
                    tmpf->open(tempfile, open_overwrite);

                    tempfile_ = tempfile;
                }
            }

            // Now write the header (either to the tempfile/memory buffer, or to the actual file)
            writeHeader();
        }
        else {
            // Non-empty file.  First check to see if it's an XZ-compressed file:
            if (checkXZ(file_size)) {
                // We preserve the compression when saving:
                compress_ = true;
                std::unique_ptr<std::iostream> tmpf;
                if (memory) {
                    tmpf.reset(new std::stringstream(open_readwrite));
                    //f_.reset(fmem);
                }
                else {
                    auto tempfile = Serialization::tempfile(filename, tmpdir);

                    auto *tmpfs = new std::fstream;
                    tmpf.reset(tmpfs);
                    tmpfs->exceptions(tmpfs->failbit | tmpfs->badbit);
                    tmpfs->open(tempfile, open_overwrite);

                    // Record that we've created this tempfile:
                    tempfile_ = tempfile;
                }

                decompressXZ(*f_, *tmpf);

                // If we change things and then save, we're going to overwrite the file:
                final_filename_ = filename;

                // Now replace the f_ pointer with with temporary one
                f_ = std::move(tmpf);
            }
            // Now either f_ is the original file or, if compressed, the decompressed content (in a
            // memory buffer or temporary file).  Either way, we should find the eris header (and
            // throw if we don't):
            readHeader();
        }
    }
    catch (std::ios_base::failure &c) {
        // Open failed: clean up any opened files/tempfiles:
        close();
        throw std::ios_base::failure("Unable to open " + filename + ": " + strerror(errno));
    }
}

void Serialization::close() {
    std::cerr << "CLOSING\n";
    // We're going to catch and rethrow any exceptions, but are going to perform cleanup before
    // rethrowing.
    bool reached_cleanup = false;
    try {
        std::cerr << "mem: " << memory_only_ << ", ro: " << read_only_ << ", ch: " << changed_ << "\n";
        // If this is an actual file which was not opened read-only and we actually wrote to it:
        if (not memory_only_ and not read_only_ and changed_) {
            if (final_f_) {
                if (compress_)
                    copyToXZ(*final_f_, xz_level);
                else
                    copyTo(*final_f_);
            }
            else if (not final_filename_.empty()) {
                if (compress_)
                    copyToXZ(final_filename_, xz_level);
                else
                    copyTo(final_filename_);
            }
            // Otherwise there's nothing to do (because we were already writing to the final
            // destination).
        }

        reached_cleanup = true;
        cleanup();
    }
    catch (...) {
        if (not reached_cleanup) cleanup();
        throw;
    }
}

void Serialization::cleanup() {
    f_.reset();
    final_f_.reset();
    final_filename_.clear();
    changed_ = false;

    if (not tempfile_.empty()) {
        std::remove(tempfile_.c_str());
        tempfile_.clear();
    }
}
            
bool Serialization::checkXZ(std::iostream::pos_type size) {
    if (size < 6) return false;

    char magic[6];
    f_->seekg(0);
    f_->read(magic, 6);
    f_->seekg(0);
    for (int i = 0; i < 6; i++) {
        if (magic[i] != xz_magic[i]) return false;
    }

    return true;
}

void Serialization::readHeader() {
    f_->seekg(0);
    char magic[4] = {0};
    f_->read(magic, 4);
    for (int i = 0; i < 4; i++)
        if (magic[i] != eris_magic[i]) throw parse_error("'eris' file signature not found");

    auto version = read<uint32_t>();
    if (version != 1) throw parse_error("Found invalid/unsupported eris file format `" + std::to_string(version) + "'");

    {
        char app_name[17] = {0};
        f_->read(app_name, 16);

        std::string expected_app_name_s = appName();
        char app_name_want[16] = {0};
        for (size_t i = 0; i < expected_app_name_s.length(); i++) {
            app_name_want[i] = expected_app_name_s[i];
        }

        for (int i = 0; i < 16; i++) {
            if (app_name[i] != app_name_want[i])
                // NB: this conversion to string isn't quite complete: it hides nulls, even though
                // we require nulls to match, but will probably be helpful in other cases:
                throw parse_error("Found invalid/unexpected application name `" + std::string(app_name) + "'");
        }
    }

    auto app_ver = read<uint32_t>();
    if (app_ver == 0) throw parse_error("Found invalid application-specific format version `0': application versions must be > 0");
    appFileVersion(app_ver); // Throws if the version is unsupported

    // Populates app_fields_
    configureHeaderFields();

    readHeaderFields();
}

void Serialization::writeHeader() {
    writef().write(eris_magic, 4); // magic header ("eris")
    *this << uint32_t(1); // version (1)

    char app_name[16] = {0};
    std::string app_name_s = appName();
    // If appName() is longer than 16, copy the first 16 characters; otherwise copy it all:
    std::copy(app_name_s.begin(), app_name_s.length() >= 16 ? app_name_s.begin() + 16 : app_name_s.end(), app_name);
    f_->write(app_name, 16);

    uint32_t ver = appFileVersion();
    if (ver == 0) throw std::logic_error("Invalid app file version `" + std::to_string(ver) + "'");
    *this << ver;

    configureHeaderFields();

    writeHeaderFields();
}


void Serialization::appFileVersion(uint32_t version) {
    if (version != appFileVersion())
        throw parse_error("application file version (" + std::to_string(version) + ") is not supported");
}

void Serialization::configureHeaderFields() {}

void Serialization::readHeaderFields() {
    for (auto &s : app_fields_) *f_ >> *s;

    header_fields_done_ = true;

    readExtraHeader();
}
void Serialization::writeHeaderFields() {
    writef();

    for (auto &s : app_fields_) *f_ << *s;

    header_fields_done_ = true;

    writeExtraHeader();
}
void Serialization::readExtraHeader() {}
void Serialization::writeExtraHeader() {}

std::iostream::pos_type Serialization::pointerChainCreate() {
    writef().seekp(0, std::ios_base::end);
    auto loc = f_->tellp();
    *this << uint32_t(0);
    char zero[8] = {0};
    for (unsigned int i = 0; i <= POINTER_BLOCK_SIZE; i++) { // <= because the first one is the next block pointer
        f_->write(zero, 8);
    }
    return loc;
}

// We write a new block first, then update the previous block pointer, then update the size so that
// if we get any IO failure, we end up with unreferenced garbage in the file, but don't have a
// broken chain: since the chain size is updated last, if that update doesn't happen, we won't
// actually ever see the added reference.
void Serialization::pointerChainAppend(std::iostream::pos_type chain, std::iostream::pos_type pointer) {
    writef().seekg(chain);
    auto size = read<uint32_t>();
    auto last_block = f_->tellg();
    // Read the chain to its last block:
    while (size >= POINTER_BLOCK_SIZE) {
        last_block = read<uint64_t>();
        if (last_block == 0) throw std::runtime_error("Unable to append to pointer chain: broken chain encountered");
        f_->seekg(last_block);
        size -= POINTER_BLOCK_SIZE;
    }
    // If the last block is full, we have to add a new block
    if (size + 1 == POINTER_BLOCK_SIZE) {
        f_->seekp(0, std::ios_base::end);
        auto next_block = f_->tellp();
        char zero[8] = {0};
        f_->write(zero, 8); // Next block pointer
        *this << uint64_t(pointer); // The one we were asked to add
        for (unsigned int i = 1; i < POINTER_BLOCK_SIZE; i++) { // the remaining empty pointers
            f_->write(zero, 8);
        }

        // We added a new block, so back up and write its location in the previous block
        f_->seekp(last_block);
        *this << uint64_t(next_block);
    }
    else {
        // Otherwise just jump to the right location and write the pointer
        f_->seekp(uint64_t(last_block) + 8 * (1 + size)); // + 1 to skip the next block pointer
        *this << uint64_t(pointer);
    }

    // Now go back to the beginning of the chain and update the chain size
    size++;
    f_->seekp(chain);
    *this << size;
}

uint32_t Serialization::pointerChainSize(std::iostream::pos_type chain) {
    f_->seekg(chain);
    return read<uint32_t>();
}

void Serialization::pointerChainSeek(std::iostream::pos_type chain, uint32_t i) {
    f_->seekg(chain);
    auto size = read<uint32_t>();
    if (i >= size) throw std::out_of_range("Requested pointer chain index " + std::to_string(i) + " does not exist");
    while (i >= POINTER_BLOCK_SIZE) {
        auto next_block = read<uint64_t>();
        if (next_block == 0) throw std::runtime_error("Unable to seek to pointer: broken chain encountered");
        f_->seekg(next_block);
        i -= POINTER_BLOCK_SIZE;
    }
    f_->seekg(8 * (1 + uint64_t(i)), std::ios_base::cur);
    auto pointer = read<uint64_t>();
    if (pointer == 0) throw std::runtime_error("Unable to seek to pointer: null chain pointer value encountered");

    // Now, finally, seek to wherever the pointer points:
    f_->seekg(pointer);
}

std::string Serialization::tempfile(const std::string &filename, const std::string &tmpdir) {
    fs::path f(filename);
    fs::path tempfile =
        (tmpdir.empty() ? f.parent_path() : fs::path(tmpdir))
        / f.filename();
    tempfile += fs::unique_path(".%%%%-%%%%-%%%%-%%%%.tmp");
    return tempfile.native();
}

void Serialization::copyTo(const std::string &filename) {
    std::ofstream out;
    out.exceptions(out.failbit | out.badbit);
    out.open(filename, open_overwrite);
    copyTo(out);
}

void Serialization::copyTo(std::ostream &out) {
    f_->seekg(0);
    out << f_->rdbuf();
}

void Serialization::copyToXZ(const std::string &filename, uint32_t level) {
    std::ofstream xzout;
    xzout.exceptions(xzout.failbit | xzout.badbit);
    xzout.open(filename, open_overwrite);
    copyToXZ(xzout, level);
}

void Serialization::copyToXZ(std::ostream &out, uint32_t level) {
    f_->seekg(0);
    compressXZ(*f_, out, level);
}

// NB: This is a private function, not a Serialization private method, because if it were a private
// method, the "lzma.h" would be required by anything including Serialization.hpp; by only including
// it in this .cpp, the header is only needed when compiling eris itself.
void processXZ(lzma_stream &strm, lzma_ret &ret, std::istream &in, std::ostream &out) {
    uint8_t inbuf[BUFSIZ], outbuf[BUFSIZ];
    strm.next_in = nullptr;
    strm.avail_in = 0;
    strm.next_out = outbuf;
    strm.avail_out = sizeof(outbuf);
    lzma_action action = LZMA_RUN;

    auto save_in_ex = in.exceptions(), save_out_ex = out.exceptions();
    // We don't want failbit in the input exceptions, because we want to be able to hit eof without
    // throwing an exception.
    in.exceptions(in.badbit);
    out.exceptions(out.badbit | out.failbit);

    try {
        while (true) {
            if (strm.avail_in == 0 and not in.eof()) {
                in.read(reinterpret_cast<char*>(inbuf), sizeof(inbuf));
                strm.next_in = inbuf;
                strm.avail_in = in.gcount();
                if (in.eof())
                    action = LZMA_FINISH;
            }

            ret = lzma_code(&strm, action);

            if (strm.avail_out == 0 or ret == LZMA_STREAM_END) {
                auto write_size = sizeof(outbuf) - strm.avail_out;
                out.write(reinterpret_cast<char*>(outbuf), write_size);

                strm.next_out = outbuf;
                strm.avail_out = sizeof(outbuf);
            }

            if (ret != LZMA_OK) {
                if (ret == LZMA_STREAM_END) break;

                throw std::runtime_error(std::string("liblzma compression/decompression failed: ") +
                        (ret == LZMA_MEM_ERROR ? "Memory allocation failed" :
                         ret == LZMA_DATA_ERROR ? "File size limits exceeded" :
			             ret == LZMA_FORMAT_ERROR ? "Input not in .xz format" :
			             ret == LZMA_OPTIONS_ERROR ? "Unsupported compression options" :
			             ret == LZMA_DATA_ERROR ? "Compressed file is corrupt" :
			             ret == LZMA_BUF_ERROR ? "Compressed file is truncated or otherwise corrupt" :
                         "An unknown error occured"));
            }
        }
    }
    catch (const std::ios_base::failure&) {
        if (not in.bad()) in.clear();
        in.exceptions(save_in_ex);
        out.exceptions(save_out_ex);
        throw;
    }
    in.clear();
    in.exceptions(save_in_ex);
    out.exceptions(save_out_ex);
}

void Serialization::compressXZ(std::istream &in, std::ostream &out, uint32_t level) {
    lzma_stream strm = LZMA_STREAM_INIT;
    // Some testing with .crstate files convinced me that -3 is optimal: it's quite fast (compared
    // to -4 and above), and the higher numbers offer only a couple extra percentage of compression
    // (and actually, -4 did worse).
    lzma_ret ret = lzma_easy_encoder(&strm, level, LZMA_CHECK_CRC64);

    if (ret != LZMA_OK)
        throw std::runtime_error(std::string("liblzma initialization failed: ") +
                (ret == LZMA_MEM_ERROR ? "Memory allocation failed" :
                 ret == LZMA_OPTIONS_ERROR ? "Specified compression level is invalid/not supported" :
                 ret == LZMA_UNSUPPORTED_CHECK ? "Specified integrity check is not supported" :
                 "An unknown error occured"));

    processXZ(strm, ret, in, out);
}

void Serialization::decompressXZ(std::istream &in, std::ostream &out) {
    lzma_stream strm = LZMA_STREAM_INIT;
    lzma_ret ret = lzma_stream_decoder(&strm, UINT64_MAX, LZMA_CONCATENATED);

    if (ret != LZMA_OK)
        throw std::runtime_error(std::string("liblzma initialization failed: ") +
                (ret == LZMA_MEM_ERROR ? "Memory allocation failed" :
                 ret == LZMA_OPTIONS_ERROR ? "Unsupported decompression flags" :
                 "An unknown error occured"));
    processXZ(strm, ret, in, out);
}

}} // namespace eris::serialize
