//[workshop_interface
namespace transactional_key_store
{
  namespace afio = BOOST_AFIO_V2_NAMESPACE;
  namespace filesystem = BOOST_AFIO_V2_NAMESPACE::filesystem;
  using BOOST_OUTCOME_V1_NAMESPACE::outcome;
  using BOOST_OUTCOME_V1_NAMESPACE::lightweight_futures::future;

  class data_store;
  //! The type of the hash
  struct hash_value_type
  {
    union
    {
      unsigned char _uint8[16];
      unsigned int _uint32[4];
      unsigned long long int _uint64[2];
    };
    constexpr hash_value_type() noexcept : _uint64{ 0, 0 } {}
    constexpr bool operator==(const hash_value_type &o) const noexcept { return _uint64[0] == o._uint64[0] && _uint64[1] == o._uint64[1]; }
    constexpr bool operator!=(const hash_value_type &o) const noexcept { return _uint64[0] != o._uint64[0] || _uint64[1] != o._uint64[1]; }
  };
  //! The kind of hash used
  enum class hash_kind_type : unsigned char
  {
    unknown = 0,
    fast,        //!< We are using SpookyHash (0.3 cycles/byte)
    quality      //!< We are using Blake2b      (3 cycles/byte)
  };
  //! Scatter buffers type
  using buffers_type = std::vector<std::pair<char *, size_t>>;
  //! Gather buffers type
  using const_buffers_type = std::vector<std::pair<const char *, size_t>>;

  //! By-hash reference to a blob
  class blob_reference
  {
    friend class data_store;
    data_store &_ds;
    hash_kind_type _hash_kind;
    hash_value_type _hash;
    afio::off_t _length;
    afio::off_t _offset;
    std::shared_ptr<const_buffers_type> _mapping;
    constexpr blob_reference(data_store &ds) noexcept : _ds(ds), _hash_kind(hash_kind_type::unknown), _length(0), _offset((afio::off_t)-1) { }
    constexpr blob_reference(data_store &ds, hash_kind_type hash_type, hash_value_type hash, afio::off_t length, afio::off_t offset) noexcept : _ds(ds), _hash_kind(hash_type), _hash(hash), _length(length), _offset(offset) { }
  public:
    ~blob_reference();

    //! True if reference is valid
    explicit operator bool() const noexcept { return _hash_kind != hash_kind_type::unknown; }
    //! True if reference is not valid
    bool operator !() const noexcept { return _hash_kind == hash_kind_type::unknown; }
    //! True if references are equal
    bool operator==(const blob_reference &o) const noexcept { return _hash_kind == o._hash_kind && _hash == o._hash && _length == o._length; }
    //! True if references are not equal
    bool operator!=(const blob_reference &o) const noexcept { return _hash_kind != o._hash_kind || _hash != o._hash || _length != o._length; }

    //! Kind of hash used
    constexpr hash_kind_type hash_kind() const noexcept { return _hash_kind; }
    //! Hash value
    constexpr hash_value_type hash_value() const noexcept { return _hash; }
    //! Length of blob
    constexpr afio::off_t size() const noexcept { return _length; }

    //! Reads the blob into the supplied scatter buffers, returning the buffers actually scattered into.
    future<buffers_type> read(buffers_type buffers, afio::off_t offset = 0) noexcept;

    //! Maps the blob into memory, returning a shared set of scattered buffers
    future<std::shared_ptr<const_buffers_type>> map(afio::off_t offset = 0, afio::off_t length = (afio::off_t) - 1) noexcept;

    std::ostream &_debugprint(std::ostream &s) const
    {
      char buffer[33];
      for (size_t n = 0; n < 16; n++)
        sprintf(buffer+n*2, "%.2x", _hash._uint8[n]);
      s << "BLOB 0x" << buffer << " length " << _length << " offset " << _offset;
      return s;
    }
  };

  namespace detail
  {
    inline size_t is_valid_name(const char *name)
    {
      static const char banned[]="<>:\"/\\|?*";
      if (!*name || *name=='.')
        throw std::invalid_argument("Name not valid");
      const char *p = name;
      for (; *p; p++)
        for (size_t n = 0; n < sizeof(banned); n++)
          if (*p == n)
            throw std::invalid_argument("Name not valid");
      return p - name;
    }
  }

  //! A valid index type
  class index_type
  {
    const char *_name;
    size_t _length;
  public:
    index_type(const char *name) : _name(name), _length(detail::is_valid_name(name)) { }
  };
  //! A valid index name
  class index_name
  {
    const char *_name;
    size_t _length;
  public:
    index_name(const char *name) : _name(name), _length(detail::is_valid_name(name)) { }
  };

  //! Potential transaction commit outcomes
  enum class transaction_status
  {
    success = 0,  //!< The transaction was successfully committed
    conflict,     //!< This transaction conflicted with another transaction (index is stale)
    stale         //!< One or more blob references could not be found (perhaps blob is too old)
  };
  //! A transaction object
  class transaction
  {
    struct transaction_private;
    std::unique_ptr<transaction_private> p;
  public:
    //! The default index which is called "default"
    static const index_name &default_index();

    //! Look up a key
    template<class U> blob_reference lookup(U &&key, const index_name &index = default_index());

    //! Adds an update to be part of the transaction
    template<class U> void add(U &&key, blob_reference value, const index_name &index = default_index());

    //! Commits the transaction, returning outcome
    future<transaction_status> commit() noexcept;
  };

  //! Implements a late durable ACID key-value blob store
  class data_store
  {
    friend class blob_reference;
    struct data_store_private;
    std::unique_ptr<data_store_private> p;
  public:
    //! This store is to be modifiable
    static constexpr size_t writeable = (1 << 0);
    //! Index updates should not complete until on physical storage
    static constexpr size_t immediately_durable_index = (1 << 1);
    //! Blob stores should not complete until on physical storage
    static constexpr size_t immediately_durable_blob = (1 << 2);

    //! Open a data store at path with disposition flags
    data_store(size_t flags = 0, filesystem::path path = "store");

    //! Store blobs. Same content blobs MAY get coalesced into the same physical storage.
    future<std::vector<blob_reference>> store_blobs(hash_kind_type hash_type, std::vector<const_buffers_type> buffers) noexcept;

    //! Find a blob
    future<blob_reference> find_blob(hash_kind_type hash_type, hash_value_type hash) noexcept;

    //! The default index which is an index of strings called "default"
    static const std::pair<index_name, index_type> default_string_index();

    //! Begin a transaction on the named indices
    future<transaction> begin_transaction(std::vector<std::pair<index_name, index_type>> indices);
  };
}
//]