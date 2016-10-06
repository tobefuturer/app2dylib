//
//  main.m
//  restore-symbol
//
//  Created by EugeneYang on 16/8/16.
//
//

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


#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <mach-o/arch.h>



#import "app2dylib_template.hpp"




NSMutableSet* rebasePointerSet = nil;


@implementation CDLCDyldInfo (app2dylib)

//categories overwrite the original method(empty implement) to get callback for every rebase address
- (void)rebaseAddress:(uint64_t)address type:(uint8_t)type;
{
    if (self.machOFile.uses64BitABI) {
        [rebasePointerSet addObject:[NSNumber numberWithUnsignedLongLong:address]];
    }
}

@end


#define APP2DYLIB_BASE_VERSION "1.0 (64 bit)"


void print_usage(void)
{
    NSLog(@
          "\n"
          "app2dylib %s\n"
          "\n"
          "Usage: app2dylib -o <dylib-output-file> <app-mach-o-file>\n"
          "\n"
          "    app-mach-o-file   : an executable file for app (decrypted)\n"
          "\n"
          ,
          APP2DYLIB_BASE_VERSION
          );
}






int main(int argc, char * argv[]) {
    
    
    NSString * inpath = nil;
    
    NSString * outpath = nil;
    
    int ch;
    
    struct option longopts[] = {
        { "output",                  required_argument, NULL, 'o' },
        { NULL,                      0,                 NULL, 0 },
    };
    
    
    while ( (ch = getopt_long(argc, argv, "o:", longopts, NULL)) != -1) {
        switch (ch) {
            case 'o':
                outpath = [NSString stringWithUTF8String:optarg];
                break;
            default:
                break;
        }
    }
    
    if (optind < argc) {
        inpath = [NSString stringWithUTF8String:argv[optind]];
    }
    
    
    if (inpath.length == 0 || outpath.length == 0) {
        print_usage();
        exit(1);
    }
    
    NSFileManager *fileManager = [NSFileManager defaultManager];
    
    if (![fileManager fileExistsAtPath:inpath]) {
        NSLog(@"%@ not exist.", inpath);
        exit(1);
    }
 
    
    rebasePointerSet = [NSMutableSet set];
    
    CDFile * ofile = [CDFile fileWithContentsOfFile:inpath searchPathState:nil];
    
    
    if ([ofile isKindOfClass:[CDFatFile class]] ) {
        NSLog(@"app2dylib supports armv7 and arm64 archtecture, but not support fat file. Please use lipo to thin the image file first.");
        exit(1);
    }
    
    

    
    CDMachOFile * machOFile = (CDMachOFile *)ofile;
    if ((machOFile.flags & MH_PIE) == 0){
        NSLog(@"%@ is not a position-independent executable, can\'t convert to dylib.", inpath);
        exit(1);
    }

    const bool Is32Bit = ! machOFile.uses64BitABI;
    if (Is32Bit) {
        app2dylib< uint32_t , struct mach_header, struct segment_command, struct section, struct nlist, struct dylib_command>(inpath, outpath);
    } else {
        app2dylib< uint64_t , struct mach_header_64, struct segment_command_64, struct section_64, struct nlist_64, struct dylib_command>(inpath, outpath);
    }
    
    NSLog(@"Remember to codesign the generated dylib.");
    fprintf(stderr,"=========== Finish ============\n"); 
}
