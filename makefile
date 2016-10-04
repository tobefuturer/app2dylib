
.PHONY:app2dylib

TMP_FILE := libMachObjC.a app2dylib.dSYM/ build/

restore-symbol: 
	rm -f app2dylib
	xcodebuild -project "app2dylib.xcodeproj" -target "app2dylib" -configuration "Release" CONFIGURATION_BUILD_DIR="$(shell pwd)" -jobs 4 build
	rm -rf $(TMP_FILE)
	

clean:
	rm -rf app2dylib $(TMP_FILE)

