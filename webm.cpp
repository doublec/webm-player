#include <iostream>
#include <fstream>
#include <cassert>
#define HAVE_STDINT_H 1
#define ON2_CODEC_DISABLE_COMPAT 1
extern "C" {
#include "on2_decoder.h"
#include "vp8dx.h"
#include "nestegg/nestegg.h"
}
#include <SDL/SDL.h>

using namespace std;

#define interface (&on2_codec_vp8_dx_algo)

#define IVF_FILE_HDR_SZ  (32)
#define IVF_FRAME_HDR_SZ (12)

static unsigned int mem_get_le32(const unsigned char *mem) {
    return (mem[3] << 24)|(mem[2] << 16)|(mem[1] << 8)|(mem[0]);
}


void play_vp8(char const* name);
void play_webm(char const* name);


void play_vp8(char const* name) {
  on2_codec_ctx_t  codec;
  int              flags = 0, frame_cnt = 0;
  unsigned char    file_hdr[IVF_FILE_HDR_SZ];
  unsigned char    frame_hdr[IVF_FRAME_HDR_SZ];
  unsigned char    frame[256*1024];
  on2_codec_err_t  res;

  ifstream infile(name);

  /* Read file header */
  infile.read((char*)file_hdr, IVF_FILE_HDR_SZ);
  if(!(file_hdr[0]=='D' && file_hdr[1]=='K' && file_hdr[2]=='I'
       && file_hdr[3]=='F')) {
    cerr << name << " is not an IVF file." << endl;
    return;
  }

  cout << "Using " << on2_codec_iface_name(interface) << endl;
  /* Initialize codec */                                                    
  if(on2_codec_dec_init(&codec, interface, NULL, flags)) {
    cerr << "Failed to initialize decoder" << endl;
    return;
  }

  /* Read each frame */
  while (!infile.eof()) {
    infile.read((char*)frame_hdr, IVF_FRAME_HDR_SZ);
    if (infile.gcount() != IVF_FRAME_HDR_SZ)
      break;

    int               frame_sz = mem_get_le32(frame_hdr);
    on2_codec_iter_t  iter = NULL;
    on2_image_t      *img;


    frame_cnt++;
    if(frame_sz > sizeof(frame)) {
      cerr << "Frame " << frame_sz << " data too big for example code buffer" << endl;
      return;
    }
    infile.read((char*)frame, frame_sz);
    if (infile.gcount() != frame_sz) {
      cerr << "Frame "<< frame_cnt << " failed to read complete frame" << endl;
      return;
    }

    /* Decode the frame */                                               
    if(on2_codec_decode(&codec, frame, frame_sz, NULL, 0)) {
      cerr << "Failed to decode frame" << endl;
      return;
    }

    /* Write decoded data to disk */
    while((img = on2_codec_get_frame(&codec, &iter))) {
      unsigned int plane, y;

#if 0
       for(plane=0; plane < 3; plane++) {
         unsigned char *buf =img->planes[plane]; 
                                                    
         for(y=0; y<img->d_h >> (plane?1:0); y++) {
           fwrite(buf, 1, img->d_w >> (plane?1:0), outfile); 
           buf += img->stride[plane];
         }                            
       }                               
#endif
    }      
  }
  cout << "Processed " << frame_cnt << " frames" << endl;
 
  if(on2_codec_destroy(&codec)) {
    cerr << "Failed to destroy codec" << endl;
    return;
  }

  infile.close();
}


int ifstream_read(void *buffer, size_t size, void *context) {
  ifstream* f = (ifstream*)context;
  f->read((char*)buffer, size);
  // success = 1
  // eof = 0
  // error = -1
  return f->gcount() == size ? 1 : f->eof() ? 0 : -1;
}

int ifstream_seek(int64_t n, int whence, void *context) {
  ifstream* f = (ifstream*)context;
  f->seekg(n, whence == SEEK_SET ? fstream::beg : whence == SEEK_CUR ? ios::cur : ios::end);
  return 1;
}

void play_webm(char const* name) {
  int r = 0;
  nestegg* ne;

  ifstream infile(name);
  nestegg_io ne_io;
  ne_io.read = ifstream_read;
  ne_io.seek = ifstream_seek;
  ne_io.userdata = (void*)&infile;

  r = nestegg_init(&ne, ne_io);
  assert(r == 0);  

  uint64_t duration = 0;
  r = nestegg_duration(ne, &duration);
  assert(r == 0);
  cout << "Duration: " << duration << endl;

  unsigned int ntracks = 0;
  r = nestegg_track_count(ne, &ntracks);
  assert(r == 0);
  cout << "Tracks: " << ntracks << endl;

  nestegg_video_params vparams;
  vparams.width = 0;
  vparams.height = 0;

  for (int i=0; i < ntracks; ++i) {
    int id = nestegg_track_codec_id(ne, i);
    assert(id >= 0);
    int type = nestegg_track_type(ne, i);
    cout << "Track " << i << " codec id: " << id << " type " << type << " ";
    if (type == NESTEGG_TRACK_VIDEO) {
            
      r = nestegg_track_video_params(ne, i, &vparams);
      assert(r == 0);
      cout << vparams.width << "x" << vparams.height << " ";
    }
    if (type == NESTEGG_TRACK_AUDIO) {
      double rate;
      unsigned int channels;
      r = nestegg_track_audio_params(ne, i, &rate, &channels);
      assert(r == 0);
      cout << rate << " " << channels << " channels ";
    }
    cout << endl;
  }

  on2_codec_ctx_t  codec;
  int              flags = 0, frame_cnt = 0;
  unsigned char    file_hdr[IVF_FILE_HDR_SZ];
  unsigned char    frame_hdr[IVF_FRAME_HDR_SZ];
  unsigned char    frame[256*1024];
  on2_codec_err_t  res;

  cout << "Using " << on2_codec_iface_name(interface) << endl;
  /* Initialize codec */                                                    
  if(on2_codec_dec_init(&codec, interface, NULL, flags)) {
    cerr << "Failed to initialize decoder" << endl;
    return;
  }

  SDL_Surface* surface = 0;
  SDL_Overlay* overlay = 0;

 

  int video_count = 0;
  int audio_count = 0;
  nestegg_packet* packet = 0;
  // 1 = keep calling
  // 0 = eof
  // -1 = error
  while (1) {
    r = nestegg_read_packet(ne, &packet);
    if (r == 1 && packet == 0)
      continue;
    if (r <=0)
      break;
 
    unsigned int track = 0;
    r = nestegg_packet_track(packet, &track);
    assert(r == 0);

    // TODO: workaround bug
    if (nestegg_track_type(ne, track) == NESTEGG_TRACK_VIDEO) {
      cout << "video frame: " << ++video_count << " ";
      unsigned int count = 0;
      r = nestegg_packet_count(packet, &count);
      assert(r == 0);
      cout << "Count: " << count << " ";
      int nframes = 0;

      for (int j=0; j < count; ++j) {
        unsigned char* data;
        size_t length;
        r = nestegg_packet_data(packet, j, &data, &length);
        assert(r == 0);

        cout << "length: " << length << " ";
        /* Decode the frame */                                               
        if(on2_codec_decode(&codec, data, length, NULL, 0)) {
          cerr << "Failed to decode frame" << endl;
          return;
        }
       on2_codec_iter_t  iter = NULL;
       on2_image_t      *img;

        /* Write decoded data to disk */
        while((img = on2_codec_get_frame(&codec, &iter))) {
          unsigned int plane, y;

          cout << "h: " << img->d_h << " w: " << img->d_w << endl;
          if (!surface) {
            r = SDL_Init(SDL_INIT_VIDEO);
            assert(r == 0);

            surface = SDL_SetVideoMode(vparams.width,
	    	                       vparams.height,
			               32,
			               SDL_SWSURFACE);
            assert(surface);
            overlay = SDL_CreateYUVOverlay(vparams.width,
	    			           vparams.height,
				           SDL_YV12_OVERLAY,
				           surface);
            assert(overlay);
 
          }
          nframes++;

          SDL_Rect rect;
          rect.x = 0;
          rect.y = 0;
          rect.w = vparams.width;
          rect.h = vparams.height;
    
          SDL_LockYUVOverlay(overlay);
          for (int y=0; y < img->d_h; ++y)
            memcpy(overlay->pixels[0]+(overlay->pitches[0]*y), 
	           img->planes[0]+(img->stride[0]*y), 
	           overlay->pitches[0]);
          for (int y=0; y < img->d_h>>1; ++y)
            memcpy(overlay->pixels[1]+(overlay->pitches[1]*y), 
	           img->planes[1]+(img->stride[1]*y), 
	           overlay->pitches[1]);
          for (int y=0; y < img->d_h>>1; ++y)
            memcpy(overlay->pixels[2]+(overlay->pitches[2]*y), 
	           img->planes[2]+(img->stride[2]*y), 
	           overlay->pitches[2]);
           SDL_UnlockYUVOverlay(overlay);	  
           SDL_DisplayYUVOverlay(overlay, &rect);
        }

        cout << "nframes: " << nframes;
      }

      cout << endl;
    }

    if (nestegg_track_type(ne, track) == NESTEGG_TRACK_AUDIO) {
      cout << "audio frame: " << ++audio_count << endl;
    }

    nestegg_free_packet(packet);
  }


#if 0
  /* Read each frame */
  while (!infile.eof()) {
    infile.read((char*)frame_hdr, IVF_FRAME_HDR_SZ);
    if (infile.gcount() != IVF_FRAME_HDR_SZ)
      break;

    int               frame_sz = mem_get_le32(frame_hdr);
    on2_codec_iter_t  iter = NULL;
    on2_image_t      *img;


    frame_cnt++;
    if(frame_sz > sizeof(frame)) {
      cerr << "Frame " << frame_sz << " data too big for example code buffer" << endl;
      return;
    }
    infile.read((char*)frame, frame_sz);
    if (infile.gcount() != frame_sz) {
      cerr << "Frame "<< frame_cnt << " failed to read complete frame" << endl;
      return;
    }

    /* Decode the frame */                                               
    if(on2_codec_decode(&codec, frame, frame_sz, NULL, 0)) {
      cerr << "Failed to decode frame" << endl;
      return;
    }

    /* Write decoded data to disk */
    while((img = on2_codec_get_frame(&codec, &iter))) {
      unsigned int plane, y;

#if 0
       for(plane=0; plane < 3; plane++) {
         unsigned char *buf =img->planes[plane]; 
                                                    
         for(y=0; y<img->d_h >> (plane?1:0); y++) {
           fwrite(buf, 1, img->d_w >> (plane?1:0), outfile); 
           buf += img->stride[plane];
         }                            
       }                               
#endif
    }      
  }
  cout << "Processed " << frame_cnt << " frames" << endl;
#endif 
  if(on2_codec_destroy(&codec)) {
    cerr << "Failed to destroy codec" << endl;
    return;
  }

  nestegg_destroy(ne);
  infile.close();
  if (surface) {
    SDL_FreeSurface(surface);
  }

  SDL_Quit();
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    cerr << "Usage: webm filename" << endl;
    return 1;
  }

  play_webm(argv[1]);
  

  return 0;
}
