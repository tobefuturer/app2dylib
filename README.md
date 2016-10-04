# app2dylib
A reverse engineering tool to convert iOS app to dylib 


# Usage

```

git clone --recursive https://github.com/tobefuturer/app2dylib.git
cd app2dylib && make
./app2dylib

```

example :

```

./app2dylib /tmp/AlipayWallet -o /tmp/libAlipayApp.dylib

```

You can use libAlipayApp.dylib as a normal dynamic library.

Call OC method or call C function like this:

```

#import <UIKit/UIKit.h>
#import <dlfcn.h>

#import <mach/mach.h>
#import <mach-o/loader.h>
#import <mach-o/dyld.h>

int main(int argc, char * argv[]) {
    NSString * dylibName = @"libAlipayApp";
    NSString * path = [[NSBundle mainBundle] pathForResource:dylibName ofType:@"dylib"];
    if (dlopen(path.UTF8String, RTLD_NOW) == NULL){
        NSLog(@"dlopen failed ，error %s", dlerror());
        return 0;
    };
    
    uint32_t dylib_count = _dyld_image_count();
    uint64_t slide = 0;
    for (int i = 0; i < dylib_count; i ++) {
        const char * name = _dyld_get_image_name(i);
        if ([[NSString stringWithUTF8String:name] isEqualToString:path]) {
            slide = _dyld_get_image_vmaddr_slide(i);
        }
    }
    assert(slide != 0);
    
    
    Class c = NSClassFromString(@"aluSecurity");
    NSString * plain = @"alipay";
    NSString * pubkey = @"MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDZ6i9VNEGEaZaYE7XffA9XRj15cp/ZKhHYY43EEva8LIhCWi29EREaF4JjZVMwFpUAfrL+9gpA7NMQmaMRHbrz1KHe2Ho4HpUhEac8M9zUbNvaDKSlhx0lq/15TQP+57oQbfJ9oKKd+he4Yd6jpBI3UtGmwJyN/T1S0DQ0aXR8OQIDAQAB";
    NSString * cipher = [c performSelector:NSSelectorFromString(@"rsaEncryptText:pubKey:") withObject:plain withObject:pubkey];
    NSLog(@"ciphertext： %@", cipher);
    
    typedef int (*BASE64_ENCODE_FUNC_TYPE) (char * output, int * output_size , char * input, int input_length);
    
    /** get function pointer*/
    BASE64_ENCODE_FUNC_TYPE base64_encode = (BASE64_ENCODE_FUNC_TYPE)(slide + 0xa798e4);
    char output[1000] = {0};
    int length = 1000;
    char * input = "alipay";
    base64_encode(output, & length,  input, (int)strlen(input));
    NSLog(@"base64 length: %d , %s", length,  output);
    
}


```
