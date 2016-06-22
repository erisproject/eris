#pragma once
#include <eris/serialize/serializer.hpp>
#include <memory>
#include <sstream>
#include <list>
#include <unordered_map>

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
     * \param compress_new if true, a new file should be XZ-compressed when saved.  This requires
     * the use of either an intermediate temporary file or an in-memory buffer in which to write
     * data until close() is called or the Serialization object is destroyed, at which point the
     * intermediate temporary file or in-memory buffer will be compressed and written to `filename`.
     * Has no effect when opening an existing file: it's compression status will be maintained.
     *
     * \param memory governs the use of an in-memory buffer for compressed files and new files.  If
     * opening and reading an existing, XZ-compressed file, a true value here causes decompression
     * to happen into an in-memory buffer.  Otherwise, when opening a new file (or overwriting an
     * existing one), this causes all data to be written to an in-memory buffer and written to disk
     * all at once when close() is called or the object is destroyed.  If this is false (the
     * default), compressed files will involve an intermediate temporary file; uncompressed files
     * will be read/written directly.
     *
     * \param tmpdir if non-empty, this specifies a directory in which temporary files should be
     * written; this has effect when reading or writing an XZ-compressed file when the `memory`
     * parameter is false.  If an empty string, the temporary file, if needed, will be placed in the
     * same directory as `filename` with a randomized filename.
     */
    void open(
            const std::string &filename,
            Mode mode,
            bool compress_new = true,
            bool memory = false,
            const std::string &tmpdir = "");

    /** Opens an in-memory buffer rather than a file-backed buffer.  This is considerably faster in
     * most cases, but also requires considerably more memory.
     *
     * If an existing file (or memory buffer) is open, it will be closed (or discarded).
     */
    void memory();

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

    constexpr static unsigned int POINTER_BLOCK_SIZE = 255;

    /** Creates a new pointer chain at the end of the file.  A pointer chain is a variable length
     * list of pointers that refer to locations with relevant data in a file.  For example, a
     * pointer chain is typically used to track the beginning location of state serializations; it
     * can also track other data that does not vary over time.
     *
     * Such a chain consists of an initial uint32_t count value (so that the length is always
     * known), followed by a block of pointers.  This block contains (POINTER_BLOCK_SIZE+1) uint64_t
     * file locations: locations 1 through POINTER_BLOCK_SIZE contain the POINTER_BLOCK_SIZE
     * pointers (0 if unused), while location 0 contains a pointer to the next block of pointers
     * (used when chain contains more than POINTER_BLOCK_SIZE pointers).  New pointer blocks are
     * added to the file as needed: the only thing the caller needs to track is the location of the
     * very beginning of the pointer chain, i.e. the value returned by this method.
     *
     * One an initial pointer block has been created, its location can be given to
     * pointerChainAppend() and pointerChainSeek() to add new pointers and seek to existing
     * pointers.  Seeking through the multiple blocks in a chain is performed transparently by those
     * methods: the caller can not worry about the fact that multiple blocks are used.
     *
     * A file can contain multiple independent pointer chains: each one is definied by a unique
     * initial position.
     *
     * \returns the starting location of the pointer chain.  The output filehandle (i.e. its
     * `tellp()` value) will be positioned at the end of the pointer block, i.e. at the (new) end of
     * the file.
     */
    std::iostream::pos_type pointerChainCreate();

    /** Adds a pointer to the pointer chain beginning at `chain`.  If the last block of the chain is
     * full, a new block is appended to the chain and the value is appended there.
     *
     * There is no guarantee made as to the file handle seek position at the end of this method;
     * calling code must use an appropriate seek before performing any additional IO operations.
     *
     * \param chain the initial pointer chain location
     * \param pointer the pointer value to append to the chain
     */
    void pointerChainAppend(std::iostream::pos_type chain, std::iostream::pos_type pointer);

    /** Returns the number of points in the pointer chain beginning at `chain`.  No guarantee is
     * made as to the filehandle position when this method returns.
     */
    uint32_t pointerChainSize(std::iostream::pos_type chain);

    /** Reads the `i`th pointer from the pointer chain and seeks the file input pointer to the
     * indicated position.
     *
     * Only the "get" position is set by this method, i.e. with a call to seekg().
     *
     * \param chain the initial pointer chain location
     * \param i the index of the desired pointer
     * \throws std::out_of_range if the pointer chain does not have at least `i+1` pointers.
     */
    void pointerChainSeek(std::iostream::pos_type chain, uint32_t i);

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

private:

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
    std::iostream::pos_type app_location_next_{28};

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
