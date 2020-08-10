#include "SPECK_Storage.h"

#include <cassert>
#include <cstring>
#include <fstream>

#ifdef USE_ZSTD
    #include "zstd.h"
#endif

template <typename T>
void speck::SPECK_Storage::copy_coeffs(const T& p, size_t len)
{
    assert(len > 0);
    assert(m_coeff_len == 0 || m_coeff_len == len);
    m_coeff_len = len;
#ifdef NO_CPP14
    m_coeff_buf.reset(new double[len]);
#else
    m_coeff_buf       = std::make_unique<double[]>(len);
#endif
    for (size_t i = 0; i < len; i++)
        m_coeff_buf[i] = p[i];
}
template void speck::SPECK_Storage::copy_coeffs(const buffer_type_d&, size_t);
template void speck::SPECK_Storage::copy_coeffs(const buffer_type_f&, size_t);

template <typename T>
void speck::SPECK_Storage::copy_coeffs(const T* p, size_t len)
{
    static_assert(std::is_floating_point<T>::value,
                  "!! Only floating point values are supported !!");

    assert(len > 0);
    assert(m_coeff_len == 0 || m_coeff_len == len);
    m_coeff_len = len;

#ifdef NO_CPP14
    m_coeff_buf.reset(new double[len]);
#else
    m_coeff_buf       = std::make_unique<double[]>(len);
#endif

    for (size_t i = 0; i < len; i++)
        m_coeff_buf[i] = p[i];
}
template void speck::SPECK_Storage::copy_coeffs(const double*, size_t);
template void speck::SPECK_Storage::copy_coeffs(const float*, size_t);

void speck::SPECK_Storage::take_coeffs(buffer_type_d coeffs, size_t len)
{
    assert(len > 0);
    assert(m_coeff_len == 0 || m_coeff_len == len);
    m_coeff_len = len;
    m_coeff_buf = std::move(coeffs);
}

void speck::SPECK_Storage::take_coeffs(buffer_type_f coeffs, size_t len)
{
    // Cannot really take the coeffs if the data type doesn't match.
    // So we make a copy and destroy the old memory block.
    copy_coeffs(coeffs, len);
    coeffs.reset();
}

void speck::SPECK_Storage::copy_bitstream(const std::vector<bool>& stream)
{
    m_bit_buffer = stream;
}

void speck::SPECK_Storage::take_bitstream(std::vector<bool>& stream)
{
    m_bit_buffer.resize(0);
    std::swap(m_bit_buffer, stream);
}

auto speck::SPECK_Storage::get_read_only_bitstream() const -> const std::vector<bool>&
{
    return m_bit_buffer;
}

auto speck::SPECK_Storage::get_read_only_coeffs() const -> const speck::buffer_type_d&
{
    return m_coeff_buf;
}

auto speck::SPECK_Storage::release_bitstream() -> std::vector<bool>
{
    return std::move( m_bit_buffer );
}

auto speck::SPECK_Storage::release_coeffs_double() -> speck::buffer_type_d
{
    m_coeff_len = 0;
    return std::move(m_coeff_buf);
}

auto speck::SPECK_Storage::release_coeffs_float() -> speck::buffer_type_f
{
    assert(m_coeff_len > 0);

#ifdef NO_CPP14
    buffer_type_f tmp(new float[m_coeff_len]);
#else
    buffer_type_f tmp = std::make_unique<float[]>(m_coeff_len);
#endif

    for (size_t i = 0; i < m_coeff_len; i++)
        tmp[i] = m_coeff_buf[i];
    m_coeff_buf = nullptr; // also destroy the current buffer
    return std::move(tmp);
}

void speck::SPECK_Storage::set_image_mean(double mean)
{
    m_image_mean = mean;
}
auto speck::SPECK_Storage::get_image_mean() const -> double
{
    return m_image_mean;
}

// Good solution to deal with bools and unsigned chars
// https://stackoverflow.com/questions/8461126/how-to-create-a-byte-out-of-8-bool-values-and-vice-versa
auto speck::SPECK_Storage::m_write(const buffer_type_c& header, size_t header_size,
                                   const char* filename) const -> int
{
    // This function writes 3 pieces of information to disk:
    // 1) Major version of SPECK and if the output is compressed by zstd, in 1 byte.
    // 2) SPECK header which is passed in
    // 3) SPECK bit buffer which is a data member of this class.
    // Note: 1) is always stored in plain binary form, whereas 2) and 3) can go through 
    // zstd compression, which is indicated by the first byte.

    // Sanity check on the size of bit_buffer
    if(m_bit_buffer.size() % 8 != 0)
        return 1;

    // Allocate output buffer
    const size_t speck_size = header_size + m_bit_buffer.size() / 8;

#ifdef NO_CPP14
    buffer_type_c buf(new char[speck_size]);
#else
    buffer_type_c buf = std::make_unique<char[]>(speck_size);
#endif

    // Copy over the header
    std::memcpy( buf.get(), header.get(), header_size);

    // Pack booleans to buf!
    int rv = speck::pack_booleans( buf, m_bit_buffer, header_size );
    if( rv != 0 )
        return rv;

#ifdef USE_ZSTD
    const size_t comp_buf_size = ZSTD_compressBound( speck_size );
    #ifdef NO_CPP14
        buffer_type_c comp_buf(new char[comp_buf_size]);
    #else
        buffer_type_c comp_buf = std::make_unique<char[]>(comp_buf_size);
    #endif
    const size_t comp_size = ZSTD_compress( comp_buf.get(), comp_buf_size, 
                             buf.get(), speck_size, ZSTD_CLEVEL_DEFAULT );
    if( ZSTD_isError( comp_size ) )
        return 1;
#endif

    // Write all the information to a file.
    //
    // Note on the scheme for information 1): if the value >= 128, then it's zstd compressed, 
    // and its major version == value - 128.
    // Otherwise, it's not zstd compressed, and its major version == value.
    //
    // Good introduction on file operators here: http://www.cplusplus.com/doc/tutorial/files/
    std::ofstream file(filename, std::ios::binary);
    if (file.is_open()) {

#ifdef USE_ZSTD
        std::uint8_t meta = std::uint8_t(SPECK_VERSION_MAJOR) + 128;
        file.write( reinterpret_cast<char*>(&meta), sizeof(meta) );
        file.write(comp_buf.get(), comp_size);
#else
        std::uint8_t meta = std::uint8_t(SPECK_VERSION_MAJOR);
        file.write( reinterpret_cast<char*>(&meta), sizeof(meta) );
        file.write(buf.get(), speck_size);
#endif

        file.close();
        return 0;
    } else
        return 1;
}

auto speck::SPECK_Storage::m_read(buffer_type_c& header, size_t header_size,
                                  const char* filename) -> int
{
    // Open a file and read its content
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open())
        return 1;
    file.seekg(0, file.end);
    size_t file_size = file.tellg();
    file.seekg(0, file.beg);

#ifdef NO_CPP14
    buffer_type_c file_buf(new char[file_size]);
#else
    buffer_type_c file_buf = std::make_unique<char[]>(file_size);
#endif

    file.read(file_buf.get(), file_size);
    file.close();

#ifdef USE_ZSTD
    const unsigned long long content_size = ZSTD_getFrameContentSize( file_buf.get(), file_size );
    if( content_size == ZSTD_CONTENTSIZE_ERROR || content_size == ZSTD_CONTENTSIZE_UNKNOWN )
        return 1;
#ifdef NO_CPP14
    buffer_type_c content_buf(new char[content_size]);
#else
    buffer_type_c content_buf = std::make_unique<char[]>(content_size);
#endif
    const size_t decomp_size = ZSTD_decompress( content_buf.get(), content_size, 
                               file_buf.get(), file_size );
    if( ZSTD_isError( decomp_size ) || decomp_size != content_size )
        return 1;

    // Copy over the header
    std::memcpy(header.get(), content_buf.get(), header_size);
    // Now interpret the booleans
    speck::unpack_booleans( m_bit_buffer, content_buf, content_size, header_size );
#else
    std::memcpy(header.get(), file_buf.get(), header_size);
    speck::unpack_booleans( m_bit_buffer, file_buf, file_size, header_size );
#endif

    return 0;
}

auto speck::SPECK_Storage::get_bit_buffer_size() const -> size_t
{
    return m_bit_buffer.size();
}
