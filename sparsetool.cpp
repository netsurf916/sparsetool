#include <sparse.h>

int main( int argc, char *argv[] )
{
    // Open the 1st parameter
    int fd = -1;
    bool read_mode = false;
    if( ( argc == 3 ) && ( argv[1][0] == 'r' ) ) {
        fd = open( argv[2], O_RDONLY );
        read_mode = true;
    }
    else if( ( argc == 3 ) && ( argv[1][0] == 'w' ) )
    {
        fd = open( argv[2], O_WRONLY );
    }
    else if( ( argc != 2 ) || ( argv[1][1] != 'i' ) )
    {
        printf(" Usage: %s [w|r][i] <file/device name>\n", argv[0] );
        printf( "   w - Write blocks to specified file/device\n" );
        printf( "   r - Read blocks from specified file/device\n" );
        printf( "   i - Optionally added to 'w' and 'r', e.g., 'wi'" );
        printf( "       and 'ri' to only show the information, no" ); 
        printf( "       data is output.\n" );
        printf( "   Note: data to write is read from stdin and data\n" );
        printf( "         read is written to stdout.\n\n" );
    }
    bool info_only = ( argv[1][1] == 'i' );
    if( !info_only && ( fd < 0 ) )
    {
        // Unable to open, so no need to continue
        return fd;
    }

    sparse_header_t header;
    chunk_header_t  chunk_header;
    uint64_t        block_offset = 0;
    uint64_t        offset       = 0;
    if( read_mode )
    {
        // Determine if it's a sparse image
        if( read_header( fd, header ) && valid_header( header ) )
        {
            if( info_only )
            {
                print_header( header );
            }
            while( read_chunk_header( fd, chunk_header ) && valid_chunk_header( chunk_header ) )
            {
                if( info_only )
                {
                    print_chunk_header( block_offset, chunk_header );
                }
                // chunk_header.total_sz represents the total size of the chunk, including
                // the header, so subtract out the header size to determine how far to seek
                uint64_t offset = chunk_header.total_sz - header.chunk_hdr_sz;
                if( offset > 0 )
                {
                    // TODO: dump contents to stdout
                    lseek64( fd, offset, SEEK_CUR );
                }
            }
        }
        else
        {
            // TODO: support raw images too
            printf( " [!] No valid header found!\n" );
        }
    }
    else
    {
        // Read image data from stdin (only sparse images supported)
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
                    while( !io_error && ( blocks > 0 ) )
                    {
                        char buffer[ SPARSE_BLOCK_SIZE ] = {0};
                        if( chunk_header.chunk_type == CHUNK_TYPE_RAW )
                        {
                            io_error = !read_data_retry( STDIN_FILENO, buffer, SPARSE_BLOCK_SIZE );
                            if( io_error )
                            {
                                printf( " [!] Read error!\n" );
                            }
                        }
                        if( !info_only && !io_error )
                        {
                            io_error = !write_data_retry( fd, buffer, SPARSE_BLOCK_SIZE );
                            if( io_error )
                            {
                                printf( " [!] Write error!\n" );
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
            // !!! UNTESTED !!!
            // Treat input as a raw image
            // Recover data from the header since stdin doesn't support seeking
            bool io_error = !write_data_retry( fd, ( void * )&header, sizeof( sparse_header_t ) );
            char buffer[ SPARSE_BLOCK_SIZE ] = {0};
            while( !io_error )
            {
                int64_t input_count = read( STDIN_FILENO, buffer, SPARSE_BLOCK_SIZE );
                io_error = ( input_count <= 0 );
                if( io_error )
                {
                    printf( " [!] Read error!\n" );
                }
                else
                {
                    io_error = !io_error && !write_data_retry( fd, buffer, input_count );
                    if( io_error )
                    {
                        printf( " [!] Write error!\n" );
                    }
                }
            }
        }
    }

    // Clean up and exit
    close( fd );
    return 0;
}