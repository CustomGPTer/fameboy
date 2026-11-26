/* Copyright 2026 Jonathan Jackson. SPDX-License-Identifier: MPL-2.0 */
#include <mgba/core/core.h>
#include <mgba/core/blip_buf.h>
#include <mgba/core/serialize.h>
#include <mgba-util/vfs.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#define REQUIRE(x) do{if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char** argv){
 REQUIRE(argc==4);
 for(int test=1;test<argc;test++){
  struct mCore* c=mCoreFind(argv[test]);REQUIRE(c);REQUIRE(c->init(c));mCoreInitConfig(c,"fameboy");struct mCoreOptions opts={.useBios=false,.skipBios=true,.sampleRate=48000,.audioBuffers=4096,.volume=0x100};mCoreConfigLoadDefaults(&c->config,&opts);mCoreLoadConfig(c);REQUIRE(mCoreLoadFile(c,argv[test]));
  color_t* pixels=calloc(256*224,sizeof(color_t));c->setVideoBuffer(c,pixels,256);c->setAudioBufferSize(c,4096);for(int i=0;i<2;i++)blip_set_rates(c->getAudioChannel(c,i),c->frequency(c),48000);
  c->reset(c);unsigned w,h;c->desiredVideoDimensions(c,&w,&h);REQUIRE(w==(test==3?240:160));REQUIRE(h==(test==3?160:144));
  int samples=0;for(int f=0;f<120;f++){c->runFrame(c);for(int ch=0;ch<2;ch++){short buffer[4096];samples+=blip_read_samples(c->getAudioChannel(c,ch),buffer,4096,0);}}
  REQUIRE(samples>150000);uint32_t counter=c->frameCounter(c);REQUIRE(counter>=120);
  c->setKeys(c,1);for(int f=0;f<3;f++)c->runFrame(c);uint32_t pressed=c->busRead8(c,test==3?0x02000000:0xc000);REQUIRE(!(pressed&1));c->setKeys(c,0);for(int f=0;f<3;f++)c->runFrame(c);uint32_t released=c->busRead8(c,test==3?0x02000000:0xc000);REQUIRE(released&1);
  char path[100];snprintf(path,sizeof(path),"/tmp/fameboy-check-%d.state",test);struct VFile* vf=VFileOpen(path,O_CREAT|O_RDWR|O_TRUNC);REQUIRE(vf);counter=c->frameCounter(c);REQUIRE(mCoreSaveStateNamed(c,vf,SAVESTATE_SAVEDATA|SAVESTATE_RTC));vf->close(vf);for(int f=0;f<5;f++)c->runFrame(c);vf=VFileOpen(path,O_RDONLY);REQUIRE(vf);REQUIRE(mCoreLoadStateNamed(c,vf,SAVESTATE_SAVEDATA|SAVESTATE_RTC));vf->close(vf);REQUIRE(c->frameCounter(c)==counter);
  void* save=NULL;size_t size=c->savedataClone(c,&save);REQUIRE(size>=8192);REQUIRE(((unsigned char*)save)[0]==0x42);REQUIRE(c->savedataRestore(c,save,size,true));free(save);
  uint64_t hash=1469598103934665603ULL;for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++)hash=(hash^pixels[y*256+x])*1099511628211ULL;
  printf("PASS %s: %ux%u; audio=%d; input=%u/%u; state round-trip; SRAM=%zu; frame hash=%016llx\n",argv[test],w,h,samples,pressed,released,size,(unsigned long long)hash);
  mCoreConfigDeinit(&c->config);c->deinit(c);free(pixels);
 }
 return 0;
}
