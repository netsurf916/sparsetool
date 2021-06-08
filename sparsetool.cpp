#include <sparse.hpp>

int main( int argc, char *argv[] )
{
    int fd = -1;
    bool info_only = false;
    if( ( argc == 3 ) && ( argv[1][0] == 'w' ) )
    {
        fd = open( argv[2], O_WRONLY );
        if( fd < 0 )
        {
            printf( " [!] Unable to open: \"%s\"\n", argv[2] );
            return -1;
        }
    }
    else if( ( argc == 2 ) && ( argv[1][0] == 'i' ) )
    {
        info_only = true;
    }
    else
    {
        printf( " Usage: %s [w|i] <file/device name>\n", argv[0]     );
        printf( "   w - Write blocks to specified file/device.\n"    );
        printf( "   i - Print sparse block information.\n" ); 
        printf( "   Note: Input data is read from stdin.\n\n"        );
        return 0;
    }

    sparse_header_t header;
    chunk_header_t  chunk_header;
    uint64_t        block_offset = 0;
    uint64_t        offset       = 0;

    // Read image data from stdin
    if( read_header( STDIN_FILENO, header ) && valid_header( header ) )
    {
        print_header( header );
        bool io_error = false;
        while( !io_error && read_chunk_header( STDIN_FILENO, chunk_header ) )
        {
            if( !info_only )
            {
                lseek64( fd, offset, SEEK_SET );
            }
            if( !valid_chunk_header( chunk_header ) )
            {
                printf( " [!] Invalid format!\n" );
                break;
            }
            print_chunk_header( block_offset, chunk_header );
            if( chunk_header.chunk_sz > 0 )
            {
                // Write contents to specified file/device
                uint64_t blocks = chunk_header.chunk_sz;
                char buffer[ SPARSE_BLOCK_SIZE ] = {0};
                if( chunk_header.chunk_type == CHUNK_TYPE_FILL )
                {
                    uint32_t filler = 0;
                    io_error = !read_data_retry( STDIN_FILENO, ( void * )&filler, sizeof( filler ) );
                    if( io_error )
                    {
                        printf( " [!] Read error!\n" );
                    }
                    else
                    {
                        for( int i = 0; i < ( SPARSE_BLOCK_SIZE / sizeof( filler ) ); ++i )
                        {
                            ( ( uint32_t * )buffer )[ i ] = filler;
                        }
                    }
                }
                if( chunk_header.chunk_type == CHUNK_TYPE_CRC32 )
                {
                    uint32_t crc = 0;
                    io_error = !read_data_retry( STDIN_FILENO, ( void * )&crc, sizeof( crc ) );
                    if( io_error )
                    {
                        printf( " [!] Read error!\n" );
                    }
                    continue;
                }
                while( !io_error && ( blocks > 0 ) )
                {
                    if( chunk_header.chunk_type == CHUNK_TYPE_RAW )
                    {
                        io_error = !read_data_retry( STDIN_FILENO, buffer, SPARSE_BLOCK_SIZE );
                        if( io_error )
                        {
                            printf( " [!] Read error!\n" );
                        }
                        if( !info_only && !io_error )
                        {
                            io_error = !write_data_retry( fd, buffer, SPARSE_BLOCK_SIZE );
                            if( io_error )
                            {
                                printf( " [!] Write error!\n" );
                            }
                        }
                    }
                    else if( chunk_header.chunk_type == CHUNK_TYPE_DONT_CARE )
                    {
                        // buffer is already declared as all zeros
                        if( !info_only && !io_error )
                        {
                            io_error = !write_data_retry( fd, buffer, SPARSE_BLOCK_SIZE );
                            if( io_error )
                            {
                                printf( " [!] Write error!\n" );
                            }
                        }
                    }
                    else if( chunk_header.chunk_type == CHUNK_TYPE_FILL )
                    {
                        if( !info_only && !io_error )
                        {
                            io_error = !write_data_retry( fd, buffer, SPARSE_BLOCK_SIZE );
                            if( io_error )
                            {
                                printf( " [!] Write error!\n" );
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
        if( !info_only )
        {
            io_error = !write_data_retry( fd, ( void * )&header, sizeof( sparse_header_t ) );
        }
        char buffer[ SPARSE_BLOCK_SIZE ] = {0};
        while( !io_error )
        {
            int64_t input_count = read( STDIN_FILENO, buffer, SPARSE_BLOCK_SIZE );
            io_error = ( input_count <= 0 );
            if( io_error )
            {
                printf( " [!] Read error!\n" );
            }
            else if( !info_only )
            {
                io_error = !io_error && !write_data_retry( fd, buffer, input_count );
                if( io_error )
                {
                    printf( " [!] Write error!\n" );
                }
            }
            else
            {
                printf( " [*] Read: %u bytes\n", input_count );
            }
        }
    }

    // Clean up and exit
    close( fd );
    return 0;
}
