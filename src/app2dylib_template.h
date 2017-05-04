//
//  app2dylib_template.h
//  app2dylib
//
//  Created by EugeneYang on 16/10/5.
//  Copyright © 2016年 Jun. All rights reserved.
//

#ifndef app2dylib_template_h
#define app2dylib_template_h


#import <Foundation/Foundation.h>
#include <sys/types.h>
#include <sys/stat.h>
#import "CDFile.h"
#import "CDMachOFile.h"
#import "CDLCSymbolTable.h"
#import "CDLCSegment.h"
#import "CDSymbol.h"
#import "CDLCDynamicSymbolTable.h"
#import "CDLCLinkeditData.h"
#import "CDClassDump.h"
#import "CDFatFile.h"
#import "CDLCDyldInfo.h"
#import "CDSection.h"

#define round(v,r) ( (v + (r-1) ) & (-r) )

extern NSMutableSet * rebasePointerSet;


#define PAGEZERO_SIZE_AFTER_MODIFY 0x4000





template <typename IntTypeForPointer, class mach_header_t, class segment_command_t, class section_t, class nlist_t, class dylib_command_t>
void app2dylib(NSString *inpath, NSString * outpath){
    
   
    NSMutableData * outData = [[NSMutableData alloc] initWithContentsOfFile:inpath];
    
    CDFile * ofile = [CDFile fileWithContentsOfFile:inpath searchPathState:nil];
    
    CDMachOFile * machOFile = (CDMachOFile *)ofile;
    CDLCSegment * pagezeroSeg = [machOFile segmentWithName:@"__PAGEZERO"];
    [machOFile.dyldInfo performSelector:@selector(logRebaseInfo)];
    
    auto seg_cmd = (segment_command_t *)((char *)outData.mutableBytes + pagezeroSeg.commandOffset);
    const uint64 PAGEZERO_SIZE_DELTA  = seg_cmd -> vmsize - PAGEZERO_SIZE_AFTER_MODIFY;
     
    auto header = (mach_header_t * )outData.mutableBytes;
    header -> filetype = MH_DYLIB;
    header -> flags = MH_NOUNDEFS | MH_DYLDLINK | MH_TWOLEVEL | MH_NO_REEXPORTED_DYLIBS;
     
    for (CDLCSegment *seg in [machOFile segments]) {
        auto seg_cmd = (segment_command_t *)((char *)outData.mutableBytes + seg.commandOffset);
        assert([seg.name isEqualToString:pagezeroSeg.name] || seg_cmd -> vmaddr > PAGEZERO_SIZE_DELTA);
        seg_cmd -> vmaddr -= PAGEZERO_SIZE_DELTA;
 
     
        char * segment_end = (char *)seg_cmd + (seg.cmdsize);
        for (auto sec =  (section_t *)(seg_cmd + 1); (char *)sec < (char*)segment_end; sec += 1) {
            assert(sec -> addr > PAGEZERO_SIZE_DELTA);
            sec -> addr -= PAGEZERO_SIZE_DELTA;
        }
    }
    
    auto __pagezero_seg_cmd = (segment_command_t *) ((char *)outData.mutableBytes + sizeof(mach_header_t));
    __pagezero_seg_cmd -> vmaddr = 0;
    __pagezero_seg_cmd -> vmsize = PAGEZERO_SIZE_AFTER_MODIFY;
    __pagezero_seg_cmd -> maxprot = 0x3;
    __pagezero_seg_cmd -> initprot = 0x3;
    strcpy(__pagezero_seg_cmd -> segname, "__ZERO");
    
    
    
    NSString * dylibName = [outpath lastPathComponent];
    
    NSString * dylibPath = nil;
    
    if(machOFile.minVersionIOS != nil && machOFile.minVersionMacOSX == nil) {
        dylibPath = [@"@executable_path" stringByAppendingPathComponent:dylibName];
    }
    
    if(machOFile.minVersionIOS == nil && machOFile.minVersionMacOSX != nil) {
        dylibPath = [@"@executable_path/../Resources" stringByAppendingPathComponent:dylibName];
    }
    
    if(!dylibPath){
        NSLog(@"Mach-o file must have a Load Command named LC_VERSION_MIN_MACOSX or LC_VERSION_MIN_IPHONEOS!");
        exit(1);
    }
     
    CDLCDyldInfo *dyldInfo = machOFile.dyldInfo;
    uint32 dylib_cmd_size = round((int)dylibPath.length + 1,8) + sizeof(dylib_command_t);
    auto dylib_cmd = (dylib_command_t *)((char *)outData.mutableBytes + dyldInfo.commandOffset);
    char *loadcmd_end = (char *)outData.mutableBytes +  sizeof(mach_header_t) + header -> sizeofcmds;
    
    {
        CDLCSegment *TEXT_seg = [machOFile segmentWithName:@"__TEXT"];
        CDSection *text_sec = [TEXT_seg sectionWithName:@"__text"];
        
        uint64_t fileOffset_text_sec = [text_sec fileOffsetForAddress:text_sec.addr];
        
        uint64_t need_space = sizeof(mach_header_t) + header -> sizeofcmds + dylib_cmd_size;
        if (fileOffset_text_sec < need_space) {
            NSLog(@"It seem there is not empty sapce to insert dylib_command");
            exit(1);
        }
    }
    
    
     long movesize = loadcmd_end - (char *)dylib_cmd;
     void *moveto = (char * )dylib_cmd + dylib_cmd_size;
     memmove(moveto, dylib_cmd, movesize);
     
     dylib_cmd -> cmd = LC_ID_DYLIB;
     dylib_cmd -> cmdsize = dylib_cmd_size;
     dylib_cmd -> dylib.name.offset = sizeof(dylib_command_t);
     dylib_cmd -> dylib.timestamp = 1;
     dylib_cmd -> dylib.current_version = 0x10000;
     dylib_cmd -> dylib.compatibility_version = 0x10000;
     strcpy((char *)dylib_cmd + sizeof(dylib_command_t), dylibPath.UTF8String);
     
     
     header -> ncmds += 1;
     header -> sizeofcmds += dylib_cmd_size;
     
     
     for (NSNumber * num in rebasePointerSet) {
         uint64_t addr = [num unsignedLongLongValue];
         uint64_t offset = [machOFile dataOffsetForAddress:addr];
         auto locationToFix = (IntTypeForPointer *) ((char *)outData.mutableBytes + (uint32_t)offset);
         (* locationToFix) -= PAGEZERO_SIZE_DELTA;
     }
    
    
     
    char * nlistsData = (char *)outData.mutableBytes + machOFile.symbolTable.symoff;
     
    
    nlist_t * list = (nlist_t * )nlistsData;
    for (int i = 0; i < machOFile.symbolTable.nsyms; i ++){
        if(((list[i].n_type & N_TYPE) == N_SECT) && (list[i].n_sect != NO_SECT)){
            if (list[i].n_value > PAGEZERO_SIZE_DELTA) {
                list[i].n_value -= PAGEZERO_SIZE_DELTA;
            }
        }
    }
    
    
    NSError * err = nil;
    [outData writeToFile:outpath options:NSDataWritingWithoutOverwriting error:&err];
    
    if (!err) {
        chmod(outpath.UTF8String, 0755);
    }else{
        NSLog(@"Write file error : %@", err);
        return;
    }
    
    

}



#endif /* app2dylib_template_h */
