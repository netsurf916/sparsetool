#include <sparse.hpp>

bool read_data_retry( int fd, void *buffer, uint64_t size )
{
    bool ok = true;
    int64_t data_count = read( fd, buffer, size );
    if( data_count <= 0 )
    {
        ok = false;
    }
    while( ok && data_count != size )
    {
        int64_t next_count = read( fd, ( char * )buffer + data_count, size - data_count );
        if( next_count <= 0 )
        {
            ok = false;
        }
        data_count += next_count;
    }
    return ok;
}

bool write_data_retry( int fd, void *buffer, uint64_t size )
{
    bool ok = true;
    int64_t data_count = write( fd, buffer, size );
    if( data_count <= 0 )
    {
        ok = false;
    }
    while( ok && data_count != size )
    {
        int64_t next_count = write( fd, ( char * )buffer + data_count, size - data_count );
        if( next_count <= 0 )
        {
            ok = false;
        }
        data_count += next_count;
    }
    return ok;
}

bool read_header( int fd, sparse_header_t &header )
{
    return read_data_retry( fd, ( void * )&header, sizeof( sparse_header_t ) );
}

void print_header( sparse_header_t &header )
{
    printf( " [*] Magic            : 0x%08X\n",   header.magic          );
    printf( " [*] Major Version    : %10u\n",     header.major_version  );
    printf( " [*] Minor Version    : %10u\n",     header.minor_version  );
    printf( " [*] Header Size      : %10u\n",     header.file_hdr_sz    );
    printf( " [*] Chunk Header Size: %10u\n",     header.chunk_hdr_sz   );
    printf( " [*] Block Size       : %10lu\n",    header.blk_sz         );
    printf( " [*] Total Blocks     : %10lu\n",    header.total_blks     );
    printf( " [*] Total Chunks     : %10lu\n",    header.total_chunks   );
    printf( " [*] Checksum         : 0x%08X\n\n", header.image_checksum );
}

bool valid_header( sparse_header_t &header )
{
    bool ok = true;
    ok = ok && (       SPARSE_HEADER_MAGIC == header.magic         );
    ok = ok && (                         1 == header.major_version );
    ok = ok && (                         0 == header.minor_version );
    ok = ok && (         SPARSE_BLOCK_SIZE == header.blk_sz        );
    ok = ok && ( sizeof( sparse_header_t ) == header.file_hdr_sz   );
    ok = ok && ( sizeof( chunk_header_t  ) == header.chunk_hdr_sz  );
    return ok;
}

bool read_chunk_header( int fd, chunk_header_t &header )
{
    return read_data_retry( fd, ( void * )&header, sizeof( chunk_header_t ) );
}

void print_chunk_header( uint64_t &block_offset, chunk_header_t &header, uint32_t filler )
{
    printf( " [+%08u] ", block_offset );
    block_offset += header.chunk_sz;
    switch( header.chunk_type )
    {
        case CHUNK_TYPE_RAW:
            printf( "Raw,       " );
            break;
        case CHUNK_TYPE_FILL:
            printf( "Fill,      " );
            break;
        case CHUNK_TYPE_DONT_CARE:
            printf( "Don't Care," );
            break;
        case CHUNK_TYPE_CRC32:
            printf( "CRC32,     " );
            break;
        default:
            printf( "Unknown,   " );
            break;
    }
    printf( " Reserved: 0x%04X,", header.reserved1 );
    printf( " Blocks: %6u,", header.chunk_sz );
    printf( " Total Size: %10u bytes", header.total_sz );
    if( header.chunk_type == CHUNK_TYPE_FILL )
    {
        printf( ", Filler: 0x%08X\n", filler );
    }
    else
    {
        printf( "\n" );
    }
}

bool valid_chunk_header( chunk_header_t &header )
{
    bool ok = false;
    switch( header.chunk_type )
    {
        case CHUNK_TYPE_RAW:
            ok = ( ( header.total_sz - ( header.chunk_sz * SPARSE_BLOCK_SIZE ) ) == sizeof( chunk_header_t ) );
            break;
        case CHUNK_TYPE_DONT_CARE:
            ok = ( header.total_sz == sizeof( chunk_header_t ) );
            break;
        case CHUNK_TYPE_FILL:
        case CHUNK_TYPE_CRC32:
            ok = ( header.total_sz == ( sizeof( chunk_header_t ) + 4 ) );
            break;
    }
    return ok;
}