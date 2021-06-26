#include <sparse.hpp>

int main( int argc, char *argv[] )
{
    int fd_in  = STDIN_FILENO;
    int fd_out = -1;
    bool print_info = false;
    if( ( argc == 3 ) && ( ( argv[1][0] == 'w' ) || ( argv[1][1] == 'w' ) ) )
    {
        if( ( argv[2][0] == '-' ) && ( argv[2][1] == '\0' ) )
        {
            fd_out = STDOUT_FILENO;
        }
        else
        {
            fd_out = open( argv[2], O_WRONLY );
        }
        if( fd_out < 0 )
        {
            fprintf( stderr, " [!] Unable to open: \"%s\"\n", argv[2] );
            fflush( stderr );
            return -1;
        }
        if( ( argv[1][0] == 'v' ) || ( argv[1][1] == 'v' ) )
        {
            print_info = true;
        }
    }
    else if( ( argc == 2 ) && ( argv[1][0] == 'v' ) )
    {
        print_info = true;
    }
    else
    {
        printf( " Usage: %s [w|v] <file/device name>\n", argv[0]     );
        printf( "   w - Write blocks to specified file/device.\n"    );
        printf( "   v - Show verbose output.\n" ); 
        printf( "   Note: Input data is read from stdin.\n\n"        );
        return 0;
    }

    sparse_header_t header;
    chunk_header_t  chunk_header;
    uint64_t        block_offset = 0; // Only used for printed information
    uint64_t        offset       = 0;

    // Read image data from stdin
    if( read_header( fd_in, header ) && valid_header( header ) )
    {
        if( print_info )
        {
            print_header( header );
        }
        bool io_error = false;
        while( !io_error && read_chunk_header( fd_in, chunk_header ) )
        {
            if( fd_out >= 0 )
            {
                lseek64( fd_out, offset, SEEK_SET );
            }
            if( !valid_chunk_header( chunk_header ) )
            {
                fprintf( stderr, " [!] Invalid format!\n" );
                break;
            }
            if( print_info && ( chunk_header.chunk_type != CHUNK_TYPE_FILL ) )
            {
                print_chunk_header( block_offset, chunk_header );
            }
            if( chunk_header.chunk_sz > 0 )
            {
                // Write contents to specified file/device
                uint64_t blocks = chunk_header.chunk_sz;
                char buffer[ SPARSE_BLOCK_SIZE ] = {0};
                if( chunk_header.chunk_type == CHUNK_TYPE_FILL )
                {
                    uint32_t filler = 0;
                    io_error = !read_data_retry( fd_in, ( void * )&filler, sizeof( filler ) );
                    if( io_error )
                    {
                        fprintf( stderr, " [!] Read error!\n" );
                    }
                    else
                    {
                        if( print_info )
                        {
                            print_chunk_header( block_offset, chunk_header, filler );
                        }
                        for( int i = 0; i < ( SPARSE_BLOCK_SIZE / sizeof( filler ) ); ++i )
                        {
                            ( ( uint32_t * )buffer )[ i ] = filler;
                        }
                    }
                }
                if( chunk_header.chunk_type == CHUNK_TYPE_CRC32 )
                {
                    uint32_t crc = 0;
                    io_error = !read_data_retry( fd_in, ( void * )&crc, sizeof( crc ) );
                    if( io_error )
                    {
                        fprintf( stderr, " [!] Read error!\n" );
                    }
                    continue;
                }
                while( !io_error && ( blocks > 0 ) )
                {
                    if( chunk_header.chunk_type == CHUNK_TYPE_RAW )
                    {
                        io_error = !read_data_retry( fd_in, buffer, SPARSE_BLOCK_SIZE );
                        if( io_error )
                        {
                            fprintf( stderr, " [!] Read error!\n" );
                        }
                        if( ( fd_out >= 0 ) && !io_error )
                        {
                            io_error = !write_data_retry( fd_out, buffer, SPARSE_BLOCK_SIZE );
                            if( io_error )
                            {
                                fprintf( stderr, " [!] Write error!\n" );
                            }
                        }
                    }
                    else if( chunk_header.chunk_type == CHUNK_TYPE_DONT_CARE )
                    {
                        // buffer is already declared as all zeros
                        if( ( fd_out >= 0 ) && !io_error )
                        {
                            io_error = !write_data_retry( fd_out, buffer, SPARSE_BLOCK_SIZE );
                            if( io_error )
                            {
                                fprintf( stderr, " [!] Write error!\n" );
                            }
                        }
                    }
                    else if( chunk_header.chunk_type == CHUNK_TYPE_FILL )
                    {
                        if( ( fd_out >= 0 ) && !io_error )
                        {
                            io_error = !write_data_retry( fd_out, buffer, SPARSE_BLOCK_SIZE );
                            if( io_error )
                            {
                                fprintf( stderr, " [!] Write error!\n" );
                            }
                        }
                    }
                    --blocks;
                }
            }
            offset += chunk_header.total_sz - header.chunk_hdr_sz;
        }
    }
    else
    {
        // Treat input as a raw image
        // Recover data from the header since stdin doesn't support seeking
        bool io_error = false;
        if( fd_out >= 0 )
        {
            io_error = !write_data_retry( fd_out, ( void * )&header, sizeof( sparse_header_t ) );
        }
        char buffer[ SPARSE_BLOCK_SIZE ] = {0};
        while( !io_error )
        {
            int64_t input_count = read( fd_in, buffer, SPARSE_BLOCK_SIZE );
            io_error = ( input_count <= 0 );
            if( io_error )
            {
                fprintf( stderr, " [!] Read error!\n" );
            }
            else if( fd_out >= 0 )
            {
                io_error = !io_error && !write_data_retry( fd_out, buffer, input_count );
                if( io_error )
                {
                    fprintf( stderr, " [!] Write error!\n" );
                }
            }
            else if( print_info )
            {
                printf( " [*] Read: %u bytes\n", input_count );
            }
        }
    }

    // Clean up and exit
    fflush( stderr );
    close( fd_in );
    close( fd_out );
    return 0;
}
