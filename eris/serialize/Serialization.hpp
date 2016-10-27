#pragma once
#include <eris/serialize/serializer.hpp>
#include <memory>
#include <sstream>
#include <list>
#include <unordered_map>
#include <limits>

extern "C" {
#include <lzma.h>
}

namespace eris {
/** Namespace for serialization classes.
 *
 * \sa eris::serialize::Serialization
 */
namespace serialize {

/** Base class for simulation serialization.  This class provides the basic serialization framework
 * and file structure for storing a simulation's states, relying on a subclass to implement the
 * application-specific data.
 *
 * This class is invoked indirectly via the serialize() method of eris::Simulation; it exposes no
 * public constructors.
 */
class Serialization {
public:
    /** The supported file modes passable to the constructor. */
    enum class Mode {
        /** Opens the file in read-only mode.  The file must exist and contain valid data, otherwise
         * an exception will be thrown.  Any operation that attempts to write to the file will fail.
         */
        READONLY,
        /** Opens an existing file in read-write mode.  The file must already exist and contain
         * valid data (at least a valid header), otherwise an exception will be thrown.
         */
        APPEND,
        /** Opens the file in read-write mode.  If the file exists and is non-empty, it will be
         * parsed (and so must be a valid file).  Otherwise, it will be initialized as a new data
         * file.
         */
        READWRITE,
        /** Creates a new, empty file, truncating any existing file data first if the file already
         * exists, or creating a new file if it does not.
         */
        OVERWRITE
    };

    /** Exception class thrown when the data contained in the file cannot be parsed or handled for
     * various reasons.
     */
    class parse_error : public std::runtime_error {
    public:
        parse_error(std::string msg) : std::runtime_error(msg) {}
    };

    /** Default constructor.  You must call either memory() or open() before the object can be read
     * from or written to.
     */
    Serialization();

    /** Destructor.  Calls close() to flush any temporary file or memory buffer to disk (if
     * appropriate) and discards the intermediate buffer or temporary file.
     */
    virtual ~Serialization() { close(); }

    /** The compression level to use when saving this Serialization object to a compressed file
     * (i.e. during close() or destruction).
     */
    uint32_t xz_level = 3;

    /** Opens the given file according to the requested mode.  This will either create a new file
     * and write the initial header to it, or read an existing file (and, optionally, allow
     * appending new states to the file).
     *
     * Immediately before opening and reading the file, close() is called to save and/or discard
     * existing data.  See close() for details on what this means for existing data.
     *
     * \param filename the file to open
     *
     * \param mode the mode to open the file with
     *
     * \param memory governs the use of an in-memory buffer for compressed files and new files.  If
     * opening and reading an existing, XZ-compressed file, a true value here causes decompression
     * to happen into an in-memory buffer.  For non-XZ-compressed files, and new (or overwritten)
     * files, this causes all data to be written to an in-memory buffer and written to disk
     * all at once when close() is called or the object is destroyed.  If this is false (the
     * default), compressed files will involve an intermediate temporary file; uncompressed files
     * will be read/written directly.
     *
     * \param tmpdir if non-empty, this specifies a directory in which temporary files should be
     * written; this has effect when reading or writing an XZ-compressed file when the `memory`
     * parameter is false.  If an empty string, the temporary file, if needed, will be placed in the
     * same directory as `filename` with a randomized filename.
     *
     * \param compress_new if true, a new file should be XZ-compressed when saved.  This requires
     * the use of either an intermediate temporary file or an in-memory buffer in which to write
     * data until close() is called or the Serialization object is destroyed, at which point the
     * intermediate temporary file or in-memory buffer will be compressed and written to `filename`.
     * Has no effect when opening an existing file: it's compression status will be maintained.
     */
    void open(
            const std::string &filename,
            Mode mode,
            bool memory = false,
            const std::string &tmpdir = "",
            bool compress_new = true);

    /** Opens a new in-memory buffer rather than a file-backed buffer, in read-write mode.  This is
     * considerably faster in most cases, but also requires considerably more memory.
     *
     * If an existing file (or memory buffer) is open, it will be closed (or discarded).
     */
    void memory();

    /** Reads from an existing std::stringstream in the given mode.  If the stringstream contains
     * XZ-compressed data, that data is immediately decompressed into a new stringstream (and the
     * original is closed).  Otherwise the stringstream is used as-is.
     *
     * If an existing file (or memory buffer) is open, it will be closed (or discarded).
     *
     * \param s the existing stringstream to use for storage.  If the stream contains existing
     * XZ-compressed data, that data will be decompressed into a new stringstream.
     * \param mode the mode of the resulting buffer.  Mode::OVERWRITE may not be used.  Defaults to
     * Mode::READONLY.
     */
    void memory(std::stringstream &&s, Mode mode = Mode::READONLY);

    /** Returns true if close() may take time due to required copying and/or compression.  Returns
     * false if results have been written directly to the final (uncompressed) file, or if no
     * writing is needed (i.e. the file was opened read-only, or was never changed since loading).
     */
    bool closeNeedsToCopy() const;

    /** Closes an open file.  If necessary (i.e. a file was opened using open() and a temporary file
     * or memory buffer is being used) the temporary buffer is written to the target file, possibly
     * with compression.
     *
     * The Serialization object will be unusable after this call until open() or memory() is called.
     *
     * \sa open
     */
    void close();

    /** Copies the current contents (whether file- or memory-backed) to the given file.  If the
     * target exists, it will be overwritten.
     */
    void copyTo(const std::string &filename);

    /** Copies the current contents (whether file- or memory-backed) to the given output stream.
     */
    void copyTo(std::ostream &out);

    /** Compresses the current file (or memory buffer) contents to an xz file, written at the
     * given location.  If the file already exists, it will be overwritten.  Note that only the
     * current file contents are copied: any changes made after this call will not be present in
     * the compressed file.
     *
     * \param filename the filename to write to
     * \param level the xz compression level, from 0 to 9.  Defaults to 3, which is very fast
     * compared to higher levels, and seems to compress typical eris state files almost as well
     * as 9.
     * \throws std::runtime_error (or a derived object thereof, such as std::ios_base::failure)
     * upon I/O failure or liblzma internal errors.
     */
    void copyToXZ(const std::string &filename, uint32_t level = 3);

    /** Compress the current contents (whether file- or memory-backed) and copy to the given output
     * stream.  This is just like copyToXZ, but works on an already-opened filehandle.
     */
    void copyToXZ(std::ostream &out, uint32_t level = 3);

    /** Reads all available data from the given input stream, compresses it to xz format, and
     * writes the xz data to the given output stream.
     *
     * Note that this does not attempt to seek to the beginning of in: it starts at the current
     * "get" pointer, even if that is not at the beginning.
     *
     * \param in the uncompressed source
     * \param out the ostream to write the compressed data to
     * \param level the XZ compression level, from 0 to 9.  Defaults to 3, which is much faster
     * than higher levels, but compresses typical eris data files nearly as well.
     * \throws std::runtime_error (or a derived object thereof, such as std::ios_base::failure)
     * upon I/O failure or liblzma internal errors.
     */
    static void compressXZ(std::istream &in, std::ostream &out, uint32_t level = 3);

    /** Reads compressed xz data from the given input stream (which must be already opened for
     * reading and positioned at the beginning of the xz data) and writes the decompressed data
     * to the given output stream.
     *
     * \throws std::runtime_error (or a derived object thereof, such as std::ios_base::failure)
     * upon I/O failure or liblzma internal errors.
     */
    static void decompressXZ(std::istream &in, std::ostream &out);

    /** Returns the application name as should be stored in the file.  Only the first 16 characters
     * are used.
     */
    virtual std::string appName() const = 0;

    /** Returns the application-specific version as should be stored in the file.  Must be a value
     * >= 1 (a value of 0 found in the file will cause the file to be considered invalid).
     */
    virtual uint32_t appFileVersion() const = 0;

    /** Called when parsing a file to test whether the found version is acceptable.  If not
     * acceptable, this should throw Serialization::parse_error exception.  The default
     * implementation provided throws an exception if the version does not match appFileVersion();
     * implementations wishing to support backwards-compatibility should override to not throw for
     * supported file versions, and should (as other methods are called) handle any version
     * differences as needed.
     */
    virtual void appFileVersion(uint32_t version);

    /** Adds a field to the list of header fields stored in the file.  The `store` parameter will be
     * read from when the field is written, and/or written to when the field is read, and so must
     * reference an actual, non-const storage location that remains valid.
     *
     * The added type T must have a eris::serialize::serializer<T> implementation, which must be a
     * fixed size (exposed as ::size).  Primitive floating point and integer types will work (with
     * the default serializer<T> implementation).
     *
     * Subclasses should call this method during configureHeaderFields() to ensure that all fields
     * are set up for the detected file version.
     *
     * If the same reference is given to addHeaderField multiple times, the referenced value will be
     * written multiple times when writing the header, and read multiple times when reading the
     * header.  This is generally not recommended, but is explicitly allowed.
     *
     * Throws std::logic_error if called after the header has been parsed or written.
     */
    template <typename T>
    typename std::enable_if<not std::is_const<T>::value>::type
    addHeaderField(T &store) {
        if (header_fields_done_) throw std::logic_error("Cannot add header fields after the header has been read or written");
        auto s = std::make_shared<serializer<T>>(store);
        app_fields_.emplace_back(s);
        app_locations_[&store].emplace_back(s, app_location_next_);
        app_location_next_ += serializer<T>::size;
    }

    /** Updates a header field previously added with addHeaderField.  If the header fields have not
     * yet been written (or read) from the file, this does nothing; otherwise, it rewrites the
     * current value of the reference in the appropriate location.
     *
     * \param ref the referenced value, which must be one of the same references given to
     * addHeaderField.  If the reference was given to addHeaderField multiple times, all of the
     * fields are updated.
     *
     * Unlike addHeaderField, this method allows const references, so long as the reference target
     * is the same as the (non-const) reference given to addHeaderField.
     *
     * \throws std::out_of_range if the given reference does not match any addHeaderField
     * references.
     */
    template <typename T>
    void updateHeaderField(T &store) {
        if (not header_fields_done_) return;

        writef();

        for (const auto &i : app_locations_.at(&store)) {
            f_->seekp(i.second);
            *f_ << *i.first;
        }
    }

    /** Updates *all* the header fields added with addHeaderField.  Like updateHeaderField(), this
     * does nothing if the header hasn't been written/read yet; otherwise all fields are rewritten.
     *
     * Note that this does *not* call writeExtraHeader(), as the data written there is not required
     * to be fixed size.
     */
    void updateHeaderFields();


    /** Reads a serialized value from the current file position into the given variable.  The type
     * is inferred from the variable type.
     */
    template <typename T>
    inline Serialization& operator >> (T &var) {
        *f_ >> serializer<T>(var);
        return *this;
    }

    /** Reads a serialized value of the given type from the file at its current position and returns
     * it.
     */
    template <typename T>
    inline T read() {
        T val;
        *this >> val;
        return val;
    }

    /** Serializes and writes the given value to the file at the current output position.
     */
    template <typename T>
    inline Serialization& operator << (const T &val) {
        writef() << serializer<const T>(val);
        return *this;
    }

    /** Reads one value, storing it in the given argument, of the same type as the argument, from
     * the current file position.  The type must have an eris::serialize::serializer specialization.
     */
    template <typename T>
    inline void read(T &val) { *f_ >> serializer<T>(val); }

    /** Starts a new block list at the end of the file.  A block list is a variable-length list of
     * fixed-size data elements that is allocated within the file as a linked list of data chunks
     * with each chunk containing a pointer to the next fixed block of elements.
     *
     * Each chunk of a block list has a pointer to the next block (0 if there is no next block)
     * followed by a fixed number of elements of a fixed size; the beginning of the block list is
     * preceeded with structural data.  Thus you get something like the following, where P is a file
     * position, V is a block of data, and S is the number of elements in the chain:
     *
     *     SP1[VVV...VVV]
     *     P1 -> P2[VVV...VVV]
     *     P2 -> P3[VVV...VVV]
     *     ...
     *     Pn -> 0[V...V000...]
     *
     * V is a fixed size element (or set of elements); it is up to the subclass to write the content
     * (and to make sure that it writes no more than the correct size!).
     *
     * This supports an essentially unlimited number (technically, 2^32-1) of elements: when a block
     * fills up, attempting to add another element appends a new (empty) block at the end of the
     * file and updates the previous last block's pointer to refer to it.
     *
     * A file can contain an unlimited number of block lists: each is defined by a unique starting
     * location in the file.
     *
     * When writing larger, non-fixed size records, it is often useful to store just a pointer to
     * the location of a record: in such a case, use createPointerBlock() instead, which uses this
     * method internally to store file locations in the "V" records.
     *
     * \param element_size the number of bytes used by each element.  Maximum 255: if you need
     * larger blocks, write the data directly and use a pointer list (createPointerList()) to track
     * its location.
     * \param elements_per_block the number of elements per block (maximum 65535).  A larger value
     * requires more storage space for each allocated block, but also requires less seeking to go
     * through block elements.  Defaults to 511; this results in a block size (and thus maximum
     * wasted space) of 4KiB when using an 8-byte element_size.
     *
     * \returns the starting location of the pointer block.  After this call, the file's put pointer
     * is guaranteed to be at the end of the created block, i.e. at the (new) end of the file.
     */
    uint64_t blockListCreate(uint8_t element_size, uint16_t elements_per_block = 511);

    /** Special version of blockListCreate that takes a type template; the element_size is
     * determined via `serializer<T>::size`.
     *
     * \throws std::logic_error if the serialized size is greater than 255.
     */
    template <typename T>
    uint64_t blockListCreate(uint16_t elements_per_block = 511) {
        static_assert(serializer<T>::size <= std::numeric_limits<uint8_t>::max(), "Block element size must be <= 255");
        return blockListCreate(serializer<T>::size, elements_per_block);
    }

    /** This adds a new element to the given block list and positions the "put" file pointer at the
     * beginning of the element area.  This may extend the file if a new block needs to be added to
     * the block list.
     *
     * The general use is as follows:
     *
     *     serialization.blockListAppend(block_list_start);
     *     serialization << data1 << data2 << data3;
     *
     * \param location the initial location of the block list, as was returned by blockListCreate()
     * to create the block list.
     */
    void blockListAppend(uint64_t location);

    /** Returns the number of data elements in the given block list.
     *
     * \param location the initial location of the block list, as was returned by blockListCreate()
     * to create the block list.
     *
     * \returns the number of elements in the block list.  No guarantee is made as to file pointer
     * locations after this call.
     */
    uint32_t blockListSize(uint64_t location);

    /** Seeks to an element of a block list.  Both get and put file pointers are updated to the
     * beginning of the requested element.
     *
     * \param location the initial location of the block list, as was returned by blockListCreate()
     * to create the block list.
     * \param i the desired element index.
     * \throws std::out_of_range exception if i equals or exceeds the number of elements in this
     * block list (i.e. if `i >= blockListSize(location)` would be true).
     */
    void blockListSeek(uint64_t location, uint32_t i);

    /** Seeks just beyond the initial block of the given block list.  This is designed to be useful
     * when multiple (empty) block lists are created sequentially.
     */
    void blockListSkip(uint64_t location);

    /** Iterates through a block list, calling the given callable value for each block list data
     * location.  The callable argument can return true to continue iterating, or false to stop
     * iteration.
     *
     * This method does not require any particular file pointer location at the end of the call;
     * this allows nested iteration (i.e. for handling a list of lists), among other things.  Note,
     * however, that this will only iterate through the number of elements of the list at the time
     * the method was invoked: any block list elements appending during iteration will not be
     * included.
     *
     * \param location the initial location of the block list, as was returned by blockListCreate()
     * to create the block list.
     * \param call the callable function-like object to call for each block list position.  The
     * function will be called with the index of the item, and with f_ get and put pointers set to
     * the beginning of the block item data.  Should return false to stop iterating, true to
     * continue the iteration.
     */
    void blockListIterate(uint64_t location, const std::function<bool(uint32_t)> &call);

    /** Creates a new pointer list at the end of the file.  A pointer list is a variable length
     * list of pointers that refer to locations with relevant data in a file.  For example, a
     * pointer list is typically used to track the beginning location of state serializations.  It
     * is generally intended to be used with variable length records: the record is written to the
     * file, then the beginning of that record is added to the pointer list.
     *
     * The structure of this list is handled transparently: internally it uses a block list (see
     * blockListCreate()) of file offsets and handles reading and writing those offsets.
     *
     * One an initial pointer block has been created, its location can be given to
     * pointerListAppend() and pointerListSeek() to add new pointers and seek to existing pointers.
     * Seeking through the multiple blocks in a list is performed transparently by those methods:
     * the caller need not worry or know about the fact that multiple blocks are used.
     *
     * A file can contain multiple independent pointer lists: each one is definied by a unique
     * initial position.
     *
     * \param pointers_per_block the number of pointer space to allocate in each internal block.
     * The default is 511, which results in each block being 4KiB bytes long (512 pointers in all,
     * including the pointer to the next block).
     * \returns the starting location of the pointer list.  The output filehandle (i.e. its
     * `tellp()` value) will be positioned at the end of the pointer block, i.e. at the (new) end of
     * the file after this call.
     */
    uint64_t pointerListCreate(uint16_t pointers_per_block = 511);

    /** Adds a pointer to the pointer list beginning at `location`.
     *
     * There is no guarantee made as to the file handle seek position at the end of this method;
     * calling code must use an appropriate seek before performing any additional IO operations.
     *
     * The intended logic for this is something like the following:
     *
     *     f_->seekp(0, f_->end);
     *     uint64_t data_starts = f_->tellp();
     *     *this << some << serialized << record << data;
     *     pointerListAppend(location, data_starts);
     *
     * The pointer is updated *after* the data is written to make the file more resilient to
     * filesystem errors: if an error occurs in the above, there may be junk in the file, but it
     * will be unreferenced junk and so won't cause a broken file.  ("junk" here could mean either a
     * partial or full record, and/or a partial or full pointer block).
     *
     * \param location the initial pointer list location
     * \param pointer the pointer value to append to the list
     */
    void pointerListAppend(uint64_t location, uint64_t pointer);

    /** Returns the number of pointers in the pointer list beginning at `location`.  No guarantee is
     * made as to the filehandle position after this method returns.
     *
     * This is exactly equivalent to calling blockListSize(location).
     */
    uint32_t pointerListSize(uint64_t location);

    /** Reads the `i`th pointer from the pointer chain and seeks the file get and put pointers to
     * the indicated position.
     *
     * \param location the initial pointer list location
     * \param i the index of the desired pointer
     * \throws std::out_of_range if the pointer list does not have at least `i+1` pointers.
     */
    void pointerListSeek(uint64_t location, uint32_t i);

    /** Iterates through a list of pointers, seeking to the pointed location for each one and then
     * calling the given callable function/object/lambda.  This works essentially just like
     * blockListIterate, except that instead of seeking to the block list item (where the pointer is
     * stored), this also reads that pointer and then seeks to *that* location.
     *
     * \sa blockListIterate
     */
    void pointerListIterate(uint64_t location, const std::function<bool(uint32_t)> &call);

protected:

    /** Called after opening an existing file to determine whether the file is an XZ-compressed
     * file.  This check looks for the XZ signature of the first 6 bytes consisting of:
     *     0xfd '7' 'z' 'X' 'Z' 0x0
     * Returns true if the file is at least 6 bytes long and contains this signature.  The
     * filehandle's "get" pointer will be positioned at the beginning of the file after this call.
     *
     * \param size the size of the opened file
     */
    bool checkXZ(std::iostream::pos_type size);

    /** Generates a temporary file path from the given tmpdir and filename. If tmpdir is non-empty,
     * the temp filename will use tmpdir with a randomized name based on the (final) path component
     * of filename; otherwise the temporary file will be a randomized name in the same directory as
     * filename.
     *
     * The constructed filename consists of the original filaneme with '.%%%%-%%%%-%%%%-%%%%.tmp'
     * appended, with each '%' replaced with a random hexadecimal digit ([0-9a-f]).
     */
    static std::string tempfile(const std::string &filename, const std::string &tmpdir);

    // NB: if this header structure changes, be sure to update the default app_location_next_ value.
    /** Parses a header from an existing file.
     *
     * The header contains:
     *     - 4 bytes 'e' 'r' 'i' 's'
     *     - 4 byte, uint32_t eris file format (currently 1), which is limited to 24-bit values so
     *       that the serialization always contains a 0 byte in the first 8 bytes (hence making it
     *       unlikely for the file to be misidentified as a text file).
     *     - 16 byte application identifier; typically a readable string.  Null-padded if shorter
     *       than 16 bytes.
     *     - application-specific version, uint32_t; the interpretation is up to the application.
     *     - zero or more application-specific parameters
     *
     * The remainder of the file is then up to the subclass to determine.
     */
    void readHeader();

    /** Writes the header to an empty file.  The file should be freshly opened and empty: the header
     * will be written to the current file position, which is useless if not at the beginning of the
     * file.
     *
     * \sa readHeader for the header field descriptions.
     */
    void writeHeader();

    /** Called to read the set of application-specific fields contained in the file.  This is called
     * after determining the file version (and calling appFileVersion) when parsing a file, and when
     * writing the initial file header when creating a file.
     *
     * The default implementation reads and sets fields added via addHeaderField; this can be overridden
     * to do something else entirely.
     *
     * This calls readExtraHeader() after reading the fields, which subclasses can override to read
     * extra stored content.
     */
    void readHeaderFields();

    /// Subclassable method to read any arbitrary extra header data.  By default does nothing.
    virtual void readExtraHeader();

    /** Called to write the set of application-specific fields (added with addHeaderField) in the
     * header.
     *
     * This calls writeExtraHeader() after writing the fields, which subclasses can override to
     * store extra data.
     */
    void writeHeaderFields();

    /// Subclassable method to write any arbitrary extra header data.  By default does nothing.
    virtual void writeExtraHeader();

    /** Called to configure any needed fields via addHeaderField().  This is called immediately
     * before reading or writing the file header, and will be called (for reading) after
     * appFileVersion() has been called.
     *
     * The default implementation does nothing; subclasses with fields must override.
     */
    virtual void configureHeaderFields();

    /** The filehandle used for reading/writing.  Typically an fstream or sstream (if created by
     * this class), but subclasses could use something else. */
    std::unique_ptr<std::iostream> f_;


    /** This method is (and should be) called before writing to f_: it takes care of throwing an
     * exception if the file was opened read-only, and updating the status to changed so that
     * close() will properly update the file as needed.  (The operating system may not always throw
     * an appropriate exception, particularly if the original file was compressed and f_ is actually
     * a memory buffer or temporary file, which is not actually a read-only filehandle).
     *
     * \returns a reference to the object stored in f_, so that you can use this as:
     *     writef() << blah;
     * as a shortcut for:
     *     writef();
     *     *f_ << blah;
     * It only needs to be called for the first write; subsequent writes can be written directly to
     * `*f_`.
     *
     * This is called automatically by the << operator on *this, so that, as long as output occurs
     * via `*this <<`, it doesn't need to be explicitly called.
     */
    std::iostream& writef() {
        if (read_only_) throw std::invalid_argument("unable to write: serialization file is opened read-only");
        changed_ = true;
        return *f_;
    }

    /// Flags for opening in read-only mode
    static constexpr std::ios_base::openmode open_readonly = std::ios_base::binary | std::ios_base::in;

    /// Flags for opening in overwrite mode
    static constexpr std::ios_base::openmode open_overwrite = open_readonly | std::ios_base::out | std::ios_base::trunc;

    /** Flags for opening for read-write when the file exists (if it doesn't exist, use
     * open_overwrite instead: this mode will fail if the file doesn't exist).
     */
    static constexpr std::ios_base::openmode open_readwrite = open_readonly | std::ios_base::out;

    // The location of the first header field, 28: 4 for 'eris', 4 for filever, 16 for app name, 4
    // for appver, then the header fields are written.
    static constexpr uint64_t HEADER_STARTS = 28;

private:

    // Writes an empty block at the current file location
    void blockListWriteEmptyBlock(uint16_t elements_per_block, uint8_t element_size);

    // Tracks whether header fields have been read/written; if not yet done, header field updates
    // are silently ignored (since this header is still to be written).
    bool header_fields_done_{false};

    // Stores the serializer objects for app settings in order
    std::list<std::shared_ptr<serializer_base>> app_fields_;

    // Maps a reference to its file location(s) so that values can be updated on demand via
    // updateHeaderField.  Use a list because the same reference could have been written in multiple
    // places.
    std::unordered_map<void*, std::list<std::pair<std::shared_ptr<serializer_base>, std::iostream::pos_type>>> app_locations_;

    // The next app setting location; header settings start at 28 (4 for 'eris', 4 for filever, 16
    // for app name, 4 for appver).
    std::iostream::pos_type app_location_next_{HEADER_STARTS};

    // If true, this file was opened read_only_ (and so attempts to write will throw exceptions)
    bool read_only_{false};

    // If this is set, it is the open, empty final output target where f_ should be copied (or
    // compressed).
    std::unique_ptr<std::iostream> final_f_;

    // If final_f_ not set and this is non-empty, copy contents here (overwriting, and compressing
    // if compress_ is true) when closing.
    std::string final_filename_;

    // If true, the final file should be compressed (either the original was, or this is a new file
    // and compression was asked for).
    bool compress_{false};

    // This is set to true if the file content has been changed, in which case it needs to be
    // rewritten when closed (or the object is destroyed).
    bool changed_{false};

    // This is set for a pure in-memory buffer created with memory().  This is *not* set for an
    // in-memory buffer storing a decompressed encrypted file.  If this is true, calling close()
    // simply discards the buffer.
    bool memory_only_{false};

    // If non-null, this is the path to a tempfile that should be deleted when the object is
    // destroyed.
    std::string tempfile_;

    // Perform cleanup--closing filehandles, deleting tempfile_; called during close()
    void cleanup();
};

}} // namespace eris::serialize
