
// This is a quick and dirty program to remux a single Ogg Vorbis file into FSB5.
// It's easier to just use FMOD Studio to convert your Ogg Vorbis to FSB5 instead of using this…

// Based on decoder_example.c from the Vorbis library

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <vorbis/codec.h>

static uint32_t crc32_for_byte(uint32_t r) {
	for(int j = 0; j < 8; ++j)
		r = (r & 1? 0: (uint32_t)0xEDB88320L) ^ r >> 1;
	return r ^ (uint32_t)0xFF000000L;
}

static void crc32(const void *data, size_t n_bytes, uint32_t* crc) {
	static uint32_t table[0x100];
	if(!*table)
		for(size_t i = 0; i < 0x100; ++i)
			table[i] = crc32_for_byte(i);
	for(size_t i = 0; i < n_bytes; ++i)
		*crc = table[(uint8_t)*crc ^ ((uint8_t*)data)[i]] ^ *crc >> 8;
}

typedef struct __attribute__((__packed__)) {
	uint32_t header;
	uint32_t version;
	uint32_t num_samples;
	uint32_t sample_header_size;
	uint32_t name_table_size;
	uint32_t data_size;
	uint32_t mode;
	uint8_t zero[8];
	uint8_t hash[16];
	uint8_t dummy[8];
} fsb5_header;

typedef struct __attribute__((__packed__)) {
	uint64_t extra_param : 1;
	uint64_t frequency : 4;
	uint64_t stereo : 1;
	uint64_t data_offset : 28;
	uint64_t samples : 30;
} fsb5_sample_header;

typedef struct __attribute__((__packed__)) {
	uint32_t has_next : 1;
	uint32_t size : 24;
	uint32_t chunk_type : 7;
} fsb5_sample_extra_header;

typedef struct __attribute__((__packed__)) {
	uint8_t channels;
} fsb5_sample_extra_channels_header;

typedef struct __attribute__((__packed__)) {
	uint32_t frequency;
} fsb5_sample_extra_frequency_header;

typedef struct __attribute__((__packed__)) {
	uint32_t loop_start;
	uint32_t loop_end;
} fsb5_sample_extra_loop_header;

typedef struct __attribute__((__packed__)) {
	uint32_t crc32;
	uint32_t position_offset_table_length;
} fsb5_sample_extra_vorbis_header;

typedef struct __attribute__((__packed__)) {
	uint32_t granulepos;
	uint32_t offset;
} fsb5_sample_extra_vorbis_entry_header;

typedef struct __attribute__((__packed__)) linked_list_for_ogg_packets_struct {
	unsigned char *packet;
	long bytes;
	ogg_int64_t granulepos;
	struct linked_list_for_ogg_packets_struct *next;
} linked_list_for_ogg_packets;

int main(int argc, char** argv) {
	ogg_sync_state oy;   /* sync and verify incoming physical bitstream */
	ogg_stream_state os; /* take physical pages, weld into a logical stream of packets */
	ogg_page og;         /* one Ogg bitstream page. Vorbis packets are inside */
	ogg_packet op;       /* one raw packet of data for decode */

	vorbis_info vi;      /* struct that stores all the static vorbis bitstream settings */
	vorbis_comment vc;   /* struct that stores all the bitstream user comments */
	vorbis_dsp_state vd; /* central working state for the packet->PCM decoder */
	vorbis_block vb;     /* local working space for packet->PCM decode */

	uint32_t setup_packet_crc32;

	fsb5_header fh;
	fsb5_sample_header fsh;
	fsb5_sample_extra_header fseh_channels;
	fsb5_sample_extra_channels_header fsech;
	fsb5_sample_extra_header fseh_frequency;
	fsb5_sample_extra_frequency_header fsefh;
	fsb5_sample_extra_header fseh_loop;
	fsb5_sample_extra_loop_header fselh;
	fsb5_sample_extra_header fseh_vorbisdata;
	fsb5_sample_extra_vorbis_header fsevh;
	fsb5_sample_extra_vorbis_entry_header *fseveh;

	linked_list_for_ogg_packets *ll;
	linked_list_for_ogg_packets *ll_last;
	linked_list_for_ogg_packets *ll_current;

	char *buffer;
	int bytes;


	/********** Decode setup ************/

	setup_packet_crc32 = 0;
	ll = NULL;
	ll_last = NULL;
	ll_current = NULL;

	ogg_sync_init(&oy); /* Now we can read pages */

	if (argc < 2)
	{
		fprintf(stderr, "Error: not enough arguments for input file\n");
		return 2;
	}
	FILE *infile = fopen(argv[1], "rb");
	if (!infile)
	{
		fprintf(stderr, "Error: could not open input file\n");
		return 1;
	}

	while (1) { /* we repeat if the bitstream is chained */
		int eos = 0;
		int i;

		/* grab some data at the head of the stream. We want the first page
			 (which is guaranteed to be small and only contain the Vorbis
			 stream initial header) We need the first page to get the stream
			 serialno. */

		/* submit a 4k block to libvorbis' Ogg layer */
		buffer = ogg_sync_buffer(&oy, 4096);
		bytes = fread(buffer, 1, 4096, infile);
		ogg_sync_wrote(&oy, bytes);

		/* Get the first page. */
		if (ogg_sync_pageout(&oy, &og) != 1) {
			/* have we simply run out of data?  If so, we're done. */
			if (bytes < 4096)
				break;

			/* error case.  Must not be Vorbis data */
			fprintf(stderr, "Input does not appear to be an Ogg bitstream.\n");
			return 1;
		}

		/* Get the serial number and set up the rest of decode. */
		/* serialno first; use it to set up a logical stream */
		ogg_stream_init(&os, ogg_page_serialno(&og));

		/* extract the initial header from the first page and verify that the Ogg bitstream is in fact Vorbis data */

		/* I handle the initial header first instead of just having the code
			 read all three Vorbis headers at once because reading the initial
			 header is an easy way to identify a Vorbis bitstream and it's
			 useful to see that functionality seperated out. */

		vorbis_info_init(&vi);
		vorbis_comment_init(&vc);
		if (ogg_stream_pagein(&os, &og) < 0) {
			/* error; stream version mismatch perhaps */
			fprintf(stderr, "Error reading first page of Ogg bitstream data.\n");
			return 1;
		}

		if (ogg_stream_packetout(&os, &op) != 1) {
			/* no page? must not be vorbis */
			fprintf(stderr, "Error reading initial header packet.\n");
			return 1;
		}

		if (vorbis_synthesis_headerin(&vi, &vc, &op) < 0) {
			/* error case; not a vorbis header */
			fprintf(stderr, "This Ogg bitstream does not contain Vorbis audio data.\n");
			return 1;
		}

		/* At this point, we're sure we're Vorbis. We've set up the logical
			 (Ogg) bitstream decoder. Get the comment and codebook headers and
			 set up the Vorbis decoder */

		/* The next two packets in order are the comment and codebook headers.
			 They're likely large and may span multiple pages. Thus we read
			 and submit data until we get our two packets, watching that no
			 pages are missing. If a page is missing, error out; losing a
			 header page is the only place where missing data is fatal. */

		i = 0;
		while (i < 2) {
			while (i < 2) {
				int result = ogg_sync_pageout(&oy, &og);
				if (result == 0)
					break; /* Need more data */
				/* Don't complain about missing or corrupt data yet. We'll catch it at the packet output phase */
				if (result == 1) {
					ogg_stream_pagein(&os, &og); /* we can ignore any errors here as they'll also become apparent at packetout */
					while (i < 2) {
						result = ogg_stream_packetout(&os, &op);
						if (result == 0)
							break;
						if (result < 0) {
							/* Uh oh; data at some point was corrupted or missing! We can't tolerate that in a header.  Die. */
							fprintf(stderr, "Corrupt secondary header.  Exiting.\n");
							return 1;
						}
						result = vorbis_synthesis_headerin(&vi, &vc, &op);
						if (result < 0) {
							fprintf(stderr, "Corrupt secondary header.  Exiting.\n");
							return 1;
						}
						i++;
						if (i == 2)
						{
							setup_packet_crc32 = 0;
							crc32(op.packet, op.bytes, &setup_packet_crc32);
						}
					}
				}
			}
			/* no harm in not checking before adding more */
			buffer = ogg_sync_buffer(&oy, 4096);
			bytes = fread(buffer, 1, 4096, infile);
			if (bytes == 0 && i < 2) {
				fprintf(stderr, "End of file before finding all Vorbis headers!\n");
				return 1;
			}
			ogg_sync_wrote(&oy, bytes);
		}

		/* Throw the comments plus a few lines about the bitstream we're decoding */
		{
			char **ptr = vc.user_comments;
			while (*ptr) {
				fprintf(stderr, "%s\n", *ptr);
				++ptr;
			}
			fprintf(stderr, "\nBitstream is %d channel, %ldHz\n", vi.channels,
							vi.rate);
			fprintf(stderr, "Encoded by: %s\n\n", vc.vendor);
		}

		/* OK, got and parsed all three headers. Initialize the Vorbis packet->PCM decoder. */
		if (vorbis_synthesis_init(&vd, &vi) == 0) { /* central decode state */
			/* local state for most of the decode so multiple block decodes can proceed in parallel. */
			/* We could init multiple vorbis_block structures for vd here */
			vorbis_block_init(&vd, &vb);

			/* The rest is just a straight decode loop until end of stream */
			while (!eos) {
				while (!eos) {
					int result = ogg_sync_pageout(&oy, &og);
					if (result == 0)
						break;          /* need more data */
					if (result < 0) { /* missing or corrupt data at this page position */
						fprintf(stderr, "Corrupt or missing data in bitstream; continuing...\n");
					} else {
						ogg_stream_pagein(&os, &og); /* can safely ignore errors at this point */
						while (1) {
							result = ogg_stream_packetout(&os, &op);

							if (result == 0)
								break; /* need more data */
							if (result < 0) { /* missing or corrupt data at this page position */
								/* no reason to complain; already complained above */
							} else {
								/* we have a packet.  Decode it */
								ll_current = malloc(sizeof(linked_list_for_ogg_packets));
								memset(ll_current, 0, sizeof(linked_list_for_ogg_packets));
								ll_current->packet = malloc(op.bytes);
								memcpy(ll_current->packet, op.packet, op.bytes);
								ll_current->bytes = op.bytes;
								ll_current->granulepos = op.granulepos;
								if (!ll)
								{
									ll = ll_current;
								}
								else
								{
									ll_last->next = ll_current;
								}
								ll_last = ll_current;
							}
						}
						if (ogg_page_eos(&og))
							eos = 1;
					}
				}
				if (!eos) {
					buffer = ogg_sync_buffer(&oy, 4096);
					bytes = fread(buffer, 1, 4096, infile);
					ogg_sync_wrote(&oy, bytes);
					if (bytes == 0)
						eos = 1;
				}
			}

			/* ogg_page and ogg_packet structs always point to storage in libvorbis.  They're never freed or manipulated directly */

			vorbis_block_clear(&vb);
			vorbis_dsp_clear(&vd);
		} else {
			fprintf(stderr, "Error: Corrupt header during playback initialization.\n");
		}

		/* clean up this logical bitstream; before exit we see if we're followed by another [chained] */

		ogg_stream_clear(&os);
		vorbis_comment_clear(&vc);
#if 0
		// We need the frequency and channel information. Don't clear
		vorbis_info_clear(&vi); /* must be called last */
#endif
	}

	/* OK, clean up the framer */
	ogg_sync_clear(&oy);

	if (argc < 3)
	{
		fprintf(stderr, "Error: not enough arguments for output file\n");
		return 2;
	}
	FILE *outfile = fopen(argv[2], "wb");
	if (!outfile)
	{
		fprintf(stderr, "Error: could not open output file\n");
		return 1;
	}
	int fsb_header_size = 0;
	int fsb_sample_header_size = 0;
	int packet_count = 0;
	int total_data_size = 0;
	ll_current = ll;
	while (ll_current != NULL)
	{
		if (ll_current->granulepos != -1)
		{
			packet_count += 1;
		}
		total_data_size += sizeof(uint16_t);
		total_data_size += ll_current->bytes;
		ll_current = ll_current->next;
	}
	int total_data_padding = 32 - (total_data_size % 32);
	memset(&fh, 0, sizeof(fh));
	fh.header = 0x35425346;
	fh.version = 1;
	fh.num_samples = 1;
	fh.name_table_size = 0;
	fh.data_size = total_data_size + total_data_padding;
	fh.mode = 15; // Vorbis
	fh.zero[0] = 1; // Unknown what this does…
	// fh.hash is md5 hash of heaedr
	fsb_header_size += sizeof(fh);
	fsh.extra_param = 1;
	switch (vi.rate)
	{
		case 48000:
		{
			fsh.frequency = 9;
			break;
		}
		case 44100:
		{
			fsh.frequency = 8;
			break;
		}
		case 32000:
		{
			fsh.frequency = 7;
			break;
		}
		case 24000:
		{
			fsh.frequency = 6;
			break;
		}
		case 22050:
		{
			fsh.frequency = 5;
			break;
		}
		case 16000:
		{
			fsh.frequency = 4;
			break;
		}
		case 11025:
		{
			fsh.frequency = 3;
			break;
		}
		case 11000:
		{
			fsh.frequency = 2;
			break;
		}
		case 8000:
		{
			fsh.frequency = 1;
			break;
		}
		default:
		{
			fsh.frequency = 0;
			break;
		}
	}
	switch (vi.channels)
	{
		case 2:
		{
			fsh.stereo = 1;
			break;
		}
		case 1:
		{
			fsh.stereo = 0;
			break;
		}
		default:
		{
			fsh.stereo = 0;
			break;
		}
	}
	fsh.samples = 0;
	fsb_sample_header_size += sizeof(fsh);
	memset(&fseh_channels, 0, sizeof(fseh_channels));
	if (vi.channels != 2 && vi.channels != 1)
	{
		fseh_channels.has_next = 1;
		fseh_channels.size = sizeof(fsech);
		fseh_channels.chunk_type = 1; // channels
		fsech.channels = vi.channels;
		fsb_sample_header_size += sizeof(fseh_channels) + fseh_channels.size;
	}
	memset(&fseh_frequency, 0, sizeof(fseh_frequency));
	if (fsh.frequency == 0)
	{
		fseh_frequency.has_next = 1;
		fseh_frequency.size = sizeof(fsefh);
		fseh_frequency.chunk_type = 2; // frequency
		fsefh.frequency = vi.rate;
		fsb_sample_header_size += sizeof(fseh_frequency) + fseh_frequency.size;
	}
	memset(&fseh_loop, 0, sizeof(fseh_loop));
	if (argc >= 5)
	{
		char *end;
		fseh_loop.has_next = 1;
		fseh_loop.size = sizeof(fselh);
		fseh_loop.chunk_type = 3; // loop
		fselh.loop_start = (uint32_t)strtoul(argv[3], &end, 10);
		fselh.loop_end = (uint32_t)strtoul(argv[4], &end, 10);
		fsb_sample_header_size += sizeof(fseh_loop) + fseh_loop.size;
	}
	memset(&fseh_vorbisdata, 0, sizeof(fseh_vorbisdata));
	fseh_vorbisdata.has_next = 0;
	fseh_vorbisdata.size = sizeof(fsevh) + sizeof(fsb5_sample_extra_vorbis_entry_header) * packet_count;
	fseh_vorbisdata.chunk_type = 11; // vorbis data
	fsevh.crc32 = setup_packet_crc32;
	fsevh.position_offset_table_length = sizeof(fsb5_sample_extra_vorbis_entry_header) * packet_count;
	fsb_sample_header_size += sizeof(fseh_vorbisdata) + fseh_vorbisdata.size;
	int data_padding = 16 - ((fsb_header_size + fsb_sample_header_size) % 16);
	fsb_sample_header_size += data_padding;
	fh.sample_header_size = fsb_sample_header_size;
	fsb_header_size += fsb_sample_header_size;
	
	fseveh = malloc(sizeof(fsb5_sample_extra_vorbis_entry_header) * packet_count);
	packet_count = 0;
	ll_current = ll;
	int total_offset = 0;
	fsh.data_offset = 0;
	int max_granulepos = 0;
	while (ll_current != NULL)
	{
		if (ll_current->granulepos != -1)
		{
			fseveh[packet_count].offset = total_offset;
			fseveh[packet_count].granulepos = ll_current->granulepos;
			if (max_granulepos < ll_current->granulepos)
			{
				max_granulepos = ll_current->granulepos;
			}
			packet_count += 1;
		}
		total_offset += sizeof(uint16_t) + ll_current->bytes;
		ll_current = ll_current->next;
	}
	fsh.samples = max_granulepos;
	fwrite(&fh, 1, sizeof(fh), outfile);
	fwrite(&fsh, 1, sizeof(fsh), outfile);
	if (fseh_channels.has_next)
	{
		fwrite(&fseh_channels, 1, sizeof(fseh_channels), outfile);
		fwrite(&fsech, 1, sizeof(fsech), outfile);
	}
	if (fseh_frequency.has_next)
	{
		fwrite(&fseh_frequency, 1, sizeof(fseh_frequency), outfile);
		fwrite(&fsefh, 1, sizeof(fsefh), outfile);
	}
	if (fseh_loop.has_next)
	{
		fwrite(&fseh_loop, 1, sizeof(fseh_loop), outfile);
		fwrite(&fselh, 1, sizeof(fselh), outfile);
	}
	fwrite(&fseh_vorbisdata, 1, sizeof(fseh_vorbisdata), outfile);
	fwrite(&fsevh, 1, sizeof(fsevh), outfile);
	fwrite(fseveh, 1, sizeof(fsb5_sample_extra_vorbis_entry_header) * packet_count, outfile);
	uint8_t data_padding_arr[32];
	memset(data_padding_arr, 0, sizeof(data_padding_arr));
	fwrite(data_padding_arr, 1, data_padding, outfile);
	ll_current = ll;
	while (ll_current != NULL)
	{
		uint16_t packet_length;
		packet_length = ll_current->bytes;
		fwrite(&packet_length, 1, sizeof(packet_length), outfile);
		fwrite(ll_current->packet, 1, ll_current->bytes, outfile);
		ll_current = ll_current->next;
	}
	fwrite(data_padding_arr, 1, total_data_padding, outfile);

	fprintf(stderr, "Done.\n");
	return (0);
}
