/* Copyright 2026 Jonathan Jackson. SPDX-License-Identifier: MPL-2.0 */
#include <jni.h>
#include <android/bitmap.h>
#include <mgba/core/core.h>
#include <mgba/core/blip_buf.h>
#include <mgba/core/serialize.h>
#include <mgba/core/cheats.h>
#include <mgba-util/vfs.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct mCore* core;
static color_t pixels[256 * 224];
static unsigned width, height;
static char savepath[4096];
static struct mCheatDevice* active_cheats;
static void clear_cheats(struct mCheatDevice* device){
 if(!device)return;
 while(mCheatSetsSize(&device->cheats)){
  struct mCheatSet* set=*mCheatSetsGetPointer(&device->cheats,0);
  set->enabled=false;mCheatRefresh(device,set);mCheatRemoveSet(device,set);mCheatSetDeinit(set);
 }
}

static void close_core(void) {
 if (core) { clear_cheats(active_cheats);active_cheats=NULL;mCoreConfigDeinit(&core->config); core->deinit(core); core=NULL; }
}
static int flush_save(void) {
 if (!core) return 0;
 void* bytes=NULL; size_t size=core->savedataClone(core,&bytes);
 if (!size) { free(bytes); return 1; }
 char temp[4120]; snprintf(temp,sizeof(temp),"%s.tmp",savepath);
 FILE* f=fopen(temp,"wb"); int ok=0;
 if (f) { ok=fwrite(bytes,1,size,f)==size; if(fflush(f)||fsync(fileno(f)))ok=0; if(fclose(f))ok=0; if(ok)ok=!rename(temp,savepath); }
 free(bytes); if(!ok)unlink(temp); return ok;
}
JNIEXPORT jint JNICALL Java_uk_jj_fameboy_Native_open(JNIEnv* e,jclass c,jstring path,jstring save) {
 (void)c; close_core();
 const char* p=(*e)->GetStringUTFChars(e,path,0); const char* s=(*e)->GetStringUTFChars(e,save,0);
 core=mCoreFind(p); snprintf(savepath,sizeof(savepath),"%s",s);
 int result=0;
 if(core && core->init(core)) {
  mCoreInitConfig(core,"fameboy");
  struct mCoreOptions opts={.useBios=false,.skipBios=true,.sampleRate=48000,.audioBuffers=2048,.volume=0x100,.frameskip=0};
  mCoreConfigLoadDefaults(&core->config,&opts); mCoreLoadConfig(core);
  if(mCoreLoadFile(core,p)) {
   core->setVideoBuffer(core,pixels,256); core->setAudioBufferSize(core,4096);
   blip_set_rates(core->getAudioChannel(core,0),core->frequency(core),48000);
   blip_set_rates(core->getAudioChannel(core,1),core->frequency(core),48000);
   struct VFile* vf=VFileOpen(s,O_CREAT|O_RDWR);
   if(vf && !core->loadSave(core,vf))vf->close(vf);
   core->reset(core); core->desiredVideoDimensions(core,&width,&height);
   if(width<=256 && height<=224)result=(width<<16)|height;
  }
 }
 if(!result)close_core();
 (*e)->ReleaseStringUTFChars(e,path,p);(*e)->ReleaseStringUTFChars(e,save,s);return result;
}
JNIEXPORT jint JNICALL Java_uk_jj_fameboy_Native_frame(JNIEnv* e,jclass c,jobject bitmap,jshortArray audio,jint keys) {
 (void)c;if(!core)return -1;core->setKeys(core,(uint32_t)keys);core->runFrame(core);
 AndroidBitmapInfo info;void* out;
 if(AndroidBitmap_getInfo(e,bitmap,&info)==0 && info.width==width && info.height==height && AndroidBitmap_lockPixels(e,bitmap,&out)==0) {
  for(unsigned y=0;y<height;y++){uint32_t* row=(uint32_t*)((char*)out+y*info.stride);for(unsigned x=0;x<width;x++)row[x]=pixels[y*256+x]|0xFF000000;}
  AndroidBitmap_unlockPixels(e,bitmap);
 }
 int count=blip_samples_avail(core->getAudioChannel(core,0));
 int max=(*e)->GetArrayLength(e,audio)/2;if(count>max)count=max;
 jshort* samples=(*e)->GetShortArrayElements(e,audio,NULL);if(!samples)return -1;
 blip_read_samples(core->getAudioChannel(core,0),samples,count,1);blip_read_samples(core->getAudioChannel(core,1),samples+1,count,1);
 (*e)->ReleaseShortArrayElements(e,audio,samples,0);return count*2;
}
JNIEXPORT jdouble JNICALL Java_uk_jj_fameboy_Native_fps(JNIEnv* e,jclass c){(void)e;(void)c;return core?(double)core->frequency(core)/core->frameCycles(core):59.7275;}
JNIEXPORT jboolean JNICALL Java_uk_jj_fameboy_Native_flush(JNIEnv* e,jclass c){(void)e;(void)c;return flush_save();}
JNIEXPORT void JNICALL Java_uk_jj_fameboy_Native_close(JNIEnv* e,jclass c){(void)e;(void)c;close_core();}
JNIEXPORT jboolean JNICALL Java_uk_jj_fameboy_Native_state(JNIEnv* e,jclass c,jstring path,jboolean load) {
 (void)c;if(!core)return 0;const char* p=(*e)->GetStringUTFChars(e,path,0);char tmp[4120];snprintf(tmp,sizeof(tmp),"%s.tmp",p);
 struct VFile* vf=VFileOpen(load?p:tmp,load?O_RDONLY:(O_RDWR|O_CREAT|O_TRUNC));bool ok=false;
 if(vf){ok=load?mCoreLoadStateNamed(core,vf,SAVESTATE_SAVEDATA|SAVESTATE_RTC):mCoreSaveStateNamed(core,vf,SAVESTATE_SAVEDATA|SAVESTATE_RTC|SAVESTATE_METADATA);vf->close(vf);if(!load && ok)ok=!rename(tmp,p);}
 if(!load && !ok)unlink(tmp);(*e)->ReleaseStringUTFChars(e,path,p);return ok;
}
JNIEXPORT jstring JNICALL Java_uk_jj_fameboy_Native_entries(JNIEnv* e,jclass c,jstring path) {
 (void)c;const char* p=(*e)->GetStringUTFChars(e,path,0);struct VDir* d=VDirOpenArchive(p);char* text=calloc(1,262144);size_t len=0;
 if(d){struct VDirEntry* en;int n=0;while((en=d->listNext(d))&&n++<10000){const char* name=en->name(en);size_t size=strlen(name);if(en->type(en)==VFS_FILE && size<1024 && len+size+2<262144 && !strchr(name,'\n')){memcpy(text+len,name,size);len+=size;text[len++]='\n';}}d->close(d);}
 jstring r=(*e)->NewStringUTF(e,text);free(text);(*e)->ReleaseStringUTFChars(e,path,p);return r;
}
JNIEXPORT jboolean JNICALL Java_uk_jj_fameboy_Native_extract(JNIEnv* e,jclass c,jstring archive,jstring entry,jstring target) {
 (void)c;const char* a=(*e)->GetStringUTFChars(e,archive,0);const char* n=(*e)->GetStringUTFChars(e,entry,0);const char* t=(*e)->GetStringUTFChars(e,target,0);
 struct VDir* d=VDirOpenArchive(a);bool ok=false;
 if(d){struct VFile* in=d->openFile(d,n,O_RDONLY);if(in){ssize_t size=in->size(in);if(size>0 && size<=64*1024*1024){FILE* f=fopen(t,"wb");if(f){char buf[65536];ssize_t got,total=0;ok=true;while((got=in->read(in,buf,sizeof(buf)))>0){total+=got;if(total>64*1024*1024||fwrite(buf,1,got,f)!=(size_t)got){ok=false;break;}}if(got<0||total!=size)ok=false;if(fclose(f))ok=false;}}in->close(in);}d->close(d);}
 if(!ok)unlink(t);(*e)->ReleaseStringUTFChars(e,archive,a);(*e)->ReleaseStringUTFChars(e,entry,n);(*e)->ReleaseStringUTFChars(e,target,t);return ok;
}
JNIEXPORT jint JNICALL Java_uk_jj_fameboy_Native_probe(JNIEnv* e,jclass c,jstring path){
 (void)c;const char* p=(*e)->GetStringUTFChars(e,path,0);struct VFile* vf=VFileOpen(p,O_RDONLY);int result=0;
 if(vf){enum mPlatform platform=mCoreIsCompatible(vf);if(platform==mPLATFORM_GBA)result=3;else if(platform==mPLATFORM_GB){uint8_t color=0;vf->seek(vf,0x143,SEEK_SET);vf->read(vf,&color,1);result=(color&0x80)?2:1;}vf->close(vf);}
 (*e)->ReleaseStringUTFChars(e,path,p);return result;
}
JNIEXPORT jbyteArray JNICALL Java_uk_jj_fameboy_Native_snapshot(JNIEnv* e,jclass c){
 (void)c;if(!core)return NULL;struct VFile* vf=VFileMemChunk(NULL,0);if(!vf)return NULL;
 bool ok=mCoreSaveStateNamed(core,vf,SAVESTATE_SAVEDATA|SAVESTATE_RTC);ssize_t size=vf->size(vf);jbyteArray result=NULL;
 if(ok&&size>0&&size<4*1024*1024){result=(*e)->NewByteArray(e,size);if(result){void* data=vf->map(vf,size,MAP_READ);if(data)(*e)->SetByteArrayRegion(e,result,0,size,data);else result=NULL;vf->unmap(vf,data,size);}}
 vf->close(vf);return result;
}
JNIEXPORT jboolean JNICALL Java_uk_jj_fameboy_Native_restore(JNIEnv* e,jclass c,jbyteArray state){
 (void)c;if(!core||!state)return false;jsize size=(*e)->GetArrayLength(e,state);if(size<1||size>4*1024*1024)return false;
 jbyte* data=(*e)->GetByteArrayElements(e,state,NULL);if(!data)return false;struct VFile* vf=VFileFromConstMemory(data,size);bool ok=false;
 if(vf){ok=mCoreLoadStateNamed(core,vf,SAVESTATE_SAVEDATA|SAVESTATE_RTC);vf->close(vf);}(*e)->ReleaseByteArrayElements(e,state,data,JNI_ABORT);return ok;
}
JNIEXPORT jint JNICALL Java_uk_jj_fameboy_Native_cheats(JNIEnv* e,jclass c,jobjectArray lines){
 (void)c;if(!core)return -1;int n=(*e)->GetArrayLength(e,lines);if(n>50)return -1;struct mCheatDevice* device=core->cheatDevice(core);if(!device)return -1;active_cheats=device;
 struct mCheatSet* sets[50]={0};int bad=0;
 for(int i=0;i<n;i++){sets[i]=device->createSet(device,"Fame Boy");if(!sets[i]){bad=i+1;break;}jstring text=(*e)->GetObjectArrayElement(e,lines,i);const char* raw=(*e)->GetStringUTFChars(e,text,NULL);char* copy=strdup(raw);(*e)->ReleaseStringUTFChars(e,text,raw);(*e)->DeleteLocalRef(e,text);if(!copy){bad=i+1;break;}char* context=NULL;char* line=strtok_r(copy,"\n\r",&context);int count=0;
  while(line){if(*line){if(!mCheatAddLine(sets[i],line,0)){bad=i+1;break;}count++;}line=strtok_r(NULL,"\n\r",&context);}if(!count)bad=i+1;free(copy);if(bad)break;sets[i]->enabled=true;
 }
 if(bad){for(int i=0;i<n;i++)if(sets[i]){mCheatSetDeinit(sets[i]);}return bad;}
 clear_cheats(device);for(int i=0;i<n;i++){mCheatAddSet(device,sets[i]);mCheatRefresh(device,sets[i]);}return 0;
}
