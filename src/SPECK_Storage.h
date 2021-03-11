#ifndef SPECK_STORAGE_H
#define SPECK_STORAGE_H

/* 
 * This class implements mechanisms and interfaces to help 2D and 3D SPECK classes 
 * manage storage of coefficients and bitstreams.
 */

#include <vector>
#include <string>
#include <utility> // std::pair<>
#include "speck_helper.h"

namespace speck {

class SPECK_Storage {

public:

    //
    // Memory management: input and output
    // Note: the nature of the data being input and output are wavelet coefficients
    //       either from a wavelet transform (encoding) or from a bitstream (decoding).
    //
    template <typename T>
    void copy_data(const T*, size_t len);        // Make a copy of the incoming data.
    void take_data(buffer_type_d, size_t);       // Take ownership of the incoming buffer
    auto get_read_only_data() const -> std::pair<const buffer_type_d&, size_t>; // Keep ownership
    auto release_data()             -> std::pair<buffer_type_d, size_t>;     // Release ownership

    // Get the encoded bitstream.
    // The returned memory block could be written to disk by other programs.
    //
    auto get_encoded_bitstream() const -> smart_buffer_uint8;

    // Prepare internal states for a decompression operation from an encoded bitstream
    //
    // Note: it takes a raw pointer because it accesses memory provided by others,
    //       and others most likely provide a raw pointer.
    auto parse_encoded_bitstream( const void*, size_t ) -> RTNType;

    void set_image_mean(double mean);
    auto get_image_mean() const -> double;
    auto get_bit_buffer_size() const -> size_t; // Size in number of bits.
    auto get_dims() const -> std::array<size_t, 3>;

    // Given a SPECK stream, tell how long the speck stream (including header) is in bytes,
    // and what the volume/slice dimension is in num. of elements.
    // Note: don't need to provide the buffer size because this function
    //       goes to a fixed location to retrieve the stream size.
    auto get_speck_stream_size( const void* ) const -> uint64_t;
    auto get_speck_stream_dims( const void* ) const -> std::array<size_t, 3>;

protected:
    //
    // Member variables
    //
    double          m_image_mean     = 0.0;
    size_t          m_coeff_len      = 0;
    buffer_type_d   m_coeff_buf      = nullptr; // All coefficients are kept here
    const size_t    m_header_size    = 32;
    size_t          m_dim_x          = 0;
    size_t          m_dim_y          = 0;
    size_t          m_dim_z          = 0;
    int32_t         m_max_coeff_bits = 0;    // = log2(max_coefficient)

    std::vector<bool>  m_bit_buffer;

};

};

#endif
