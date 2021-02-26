#include "SPECK3D_Compressor.h"
#include "SPECK3D_Decompressor.h"
#include <cstring>
#include "gtest/gtest.h"

namespace
{

using speck::RTNType;

// Create a class that executes the entire pipeline, and calculates the error metrics
class speck_tester
{
public:
    speck_tester( const char* in, size_t x, size_t y, size_t z )
    {
        m_input_name = in;
        m_dim_x      = x;
        m_dim_y      = y;
        m_dim_z      = z;
    }

    void reset( const char* in, size_t x, size_t y, size_t z )
    {
        m_input_name = in;
        m_dim_x      = x;
        m_dim_y      = y;
        m_dim_z      = z;
    }

    float get_psnr() const
    {
        return m_psnr;
    }

    float get_lmax() const
    {
        return m_lmax;
    }

    //
    // Execute the compression/decompression pipeline. Return 0 on success
    //

#ifdef QZ_TERM
    int execute( int32_t qz_level, double tol )
#else
    int execute( float bpp )
#endif

    {
        // Reset lmax and psnr
        m_psnr = 0.0;
        m_lmax = 1000.0;
        
        const size_t  total_vals = m_dim_x * m_dim_y * m_dim_z;

        //
        // Use a compressor 
        //
        auto in_buf = speck::read_whole_file<float>( m_input_name.c_str() );
        if( in_buf.first == nullptr || in_buf.second != total_vals )
            return 1;
        SPECK3D_Compressor compressor( m_dim_x, m_dim_y, m_dim_z );
        if( compressor.copy_data( in_buf.first.get(), total_vals ) != RTNType::Good )
            return 1;

#ifdef QZ_TERM
        compressor.set_qz_level( qz_level );
        compressor.set_tolerance( tol );
#else
        compressor.set_bpp( bpp );
#endif

        if( compressor.compress() != RTNType::Good )
            return 1;
        auto stream = compressor.get_encoded_bitstream();
        if( speck::empty_buf(stream) )
            return 1;

        
        //
        // Use a decompressor 
        //
        SPECK3D_Decompressor decompressor;
        if( decompressor.use_bitstream( stream.first.get(), stream.second ) != RTNType::Good )
            return 1;
        if( decompressor.decompress() != RTNType::Good )
            return 1;
        auto vol = decompressor.get_decompressed_volume_f();
        if( vol.first == nullptr || vol.second != total_vals )
            return 1;

        //
        // Compare results
        //
        const size_t nbytes = sizeof(float) * total_vals;
        auto orig = std::make_unique<float[]>(total_vals);
        if( speck::read_n_bytes( m_input_name.c_str(), nbytes, orig.get() ) != 
            speck::RTNType::Good )
            return 1;

        float rmse, lmax, psnr, arr1min, arr1max;
        speck::calc_stats( orig.get(), vol.first.get(), total_vals,
                           &rmse, &lmax, &psnr, &arr1min, &arr1max );
        m_psnr = psnr;
        m_lmax = lmax;

        return 0;
    }


private:
    std::string m_input_name;
    size_t m_dim_x, m_dim_y, m_dim_z;
    std::string m_output_name = "output.tmp";
    float m_psnr, m_lmax;
};


#ifdef QZ_TERM
TEST( speck3d_qz_term, large_tolerance )
{
    const double tol = 1.0;
    speck_tester tester( "../test_data/wmag128.float", 128, 128, 128 );
    tester.execute( 2, tol );
    float psnr = tester.get_psnr();
    float lmax = tester.get_lmax();
    EXPECT_GT( psnr, 57.629364 );
    EXPECT_LT( lmax, tol );

    tester.execute( -1, tol );
    psnr = tester.get_psnr();
    lmax = tester.get_lmax();
    EXPECT_GT( psnr, 65.498861 );
    EXPECT_LT( lmax, tol );

    tester.execute( -2, tol );
    psnr = tester.get_psnr();
    lmax = tester.get_lmax();
    EXPECT_GT( psnr, 72.025230 );
    EXPECT_LT( lmax, 0.6164713 );
}
TEST( speck3d_qz_term, small_tolerance )
{
    const double tol = 0.07;
    speck_tester tester( "../test_data/wmag128.float", 128, 128, 128 );
    tester.execute( -3, tol );
    float psnr = tester.get_psnr();
    float lmax = tester.get_lmax();
    EXPECT_GT( psnr, 81.446037 );
    EXPECT_LT( lmax, tol );

    tester.execute( -5, tol );
    psnr = tester.get_psnr();
    lmax = tester.get_lmax();
    EXPECT_GT( psnr, 91.618080 );
    EXPECT_LT( lmax, 0.0637522 );
}
TEST( speck3d_qz_term, narrow_data_range)
{
    speck_tester tester( "../test_data/vorticity.128_128_41", 128, 128, 41 );
    tester.execute( -16, 4e-5 );
    float psnr = tester.get_psnr();
    float lmax = tester.get_lmax();
    EXPECT_GT( psnr, 42.292308 );
    EXPECT_LT( lmax, 3.168651e-5 );

    tester.execute( -18, 4e-5 );
    psnr = tester.get_psnr();
    lmax = tester.get_lmax();
    EXPECT_GT( psnr, 50.513606 );
    EXPECT_LT( lmax, 8.966978e-6 );
}
#else
TEST( speck3d_bit_rate, small )
{
    speck_tester tester( "../test_data/wmag17.float", 17, 17, 17 );

    tester.execute( 4.0f );
    float psnr = tester.get_psnr();
    float lmax = tester.get_lmax();
    EXPECT_GT( psnr, 52.893356 );
    EXPECT_LT( psnr, 52.893357 );
    EXPECT_LT( lmax, 1.5417795 );

    tester.execute( 2.0f );
    psnr = tester.get_psnr();
    lmax = tester.get_lmax();
    EXPECT_GT( psnr, 41.584476 );
    EXPECT_LT( psnr, 41.584477 );
    EXPECT_LT( lmax, 5.4159165 );

    tester.execute( 1.0f );
    psnr = tester.get_psnr();
    lmax = tester.get_lmax();
    EXPECT_GT( psnr, 34.815765 );
    EXPECT_LT( psnr, 34.815766 );
    EXPECT_LT( lmax, 12.639985 );
}


TEST( speck3d_bit_rate, big )
{
    speck_tester tester( "../test_data/wmag128.float", 128, 128, 128 );

    tester.execute( 2.0f );
    float psnr = tester.get_psnr();
    float lmax = tester.get_lmax();
    EXPECT_GT( psnr, 54.073455 );
    EXPECT_LT( psnr, 54.073456 );
    EXPECT_LT( lmax, 4.8512795 );

    tester.execute( 1.0f );
    psnr = tester.get_psnr();
    lmax = tester.get_lmax();
    EXPECT_GT( psnr, 47.296897 );
    EXPECT_LT( psnr, 47.296898 );
    EXPECT_LT( lmax, 15.9678994 );

    tester.execute( 0.5f );
    psnr = tester.get_psnr();
    lmax = tester.get_lmax();
    EXPECT_GT( psnr, 42.705162 );
    EXPECT_LT( psnr, 42.705163 );
    EXPECT_LT( lmax, 24.738228 );

    tester.execute( 0.25f );
    psnr = tester.get_psnr();
    lmax = tester.get_lmax();
    EXPECT_GT( psnr, 39.216407 );
    EXPECT_LT( psnr, 39.216408 );
    EXPECT_LT( lmax, 44.297326 );
}


TEST( speck3d_bit_rate, narrow_data_range )
{
    speck_tester tester( "../test_data/vorticity.128_128_41", 128, 128, 41 );

    tester.execute( 4.0f );
    float psnr = tester.get_psnr();
    float lmax = tester.get_lmax();
    EXPECT_GT( psnr, 69.043655 );
    EXPECT_LT( psnr, 69.043656 );
    EXPECT_LT( lmax, 9.103715e-07 );

    tester.execute( 2.0f );
    psnr = tester.get_psnr();
    lmax = tester.get_lmax();
    EXPECT_GT( psnr, 56.787048 );
    EXPECT_LT( psnr, 56.787049 );
    EXPECT_LT( lmax, 4.199554e-06 );

    tester.execute( 1.0f );
    psnr = tester.get_psnr();
    lmax = tester.get_lmax();
    EXPECT_GT( psnr, 49.777526 );
    EXPECT_LT( psnr, 49.777527 );
    EXPECT_LT( lmax, 1.002031e-05 );

    tester.execute( 0.5f );
    psnr = tester.get_psnr();
    lmax = tester.get_lmax();
    EXPECT_GT( psnr, 45.207603 );
    EXPECT_LT( psnr, 45.207604 );
    EXPECT_LT( lmax, 0.000024 );

    tester.execute( 0.25f );
    psnr = tester.get_psnr();
    lmax = tester.get_lmax();
    EXPECT_GT( psnr, 41.755619 );
    EXPECT_LT( psnr, 41.755620 );
    EXPECT_LT( lmax, 3.329716e-05 );
}
#endif

}
