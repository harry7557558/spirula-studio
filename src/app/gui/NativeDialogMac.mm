// The macOS half of NativeDialog.h: NSOpenPanel, which is main-thread only
// and runs modal, so the caller blocks until the panel is dismissed.

#import <AppKit/AppKit.h>
#if __has_include(<UniformTypeIdentifiers/UniformTypeIdentifiers.h>)
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#endif

#include <string>
#include <vector>

namespace gui {

static void set_extensions(NSSavePanel* panel, const std::vector<std::string>& extensions) {
    NSMutableArray<NSString*>* names = [NSMutableArray array];
    for (const std::string& e : extensions)
        [names addObject:[NSString stringWithUTF8String:
            e.c_str() + (!e.empty() && e[0] == '.' ? 1 : 0)]];
#if __has_include(<UniformTypeIdentifiers/UniformTypeIdentifiers.h>)
    if (@available(macOS 11.0, *)) {
        NSMutableArray<UTType*>* types = [NSMutableArray array];
        for (NSString* name in names)
            if (UTType* type = [UTType typeWithFilenameExtension:name]) [types addObject:type];
        panel.allowedContentTypes = types;
        return;
    }
#endif
    // Older SDKs and macOS versions only expose extension strings.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    panel.allowedFileTypes = names;
#pragma clang diagnostic pop
}

std::vector<std::string> mac_pick(const std::string& title, bool folder,
                                  bool also_folders,
                                  const std::vector<std::string>& extensions,
                                  const std::string& start_dir, bool multi) {
    std::vector<std::string> out;
    @autoreleasepool {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        panel.canChooseFiles = !folder;
        panel.canChooseDirectories = folder || also_folders;
        panel.canCreateDirectories = folder;
        panel.allowsMultipleSelection = multi && !folder;
        if (!title.empty())
            panel.message = [NSString stringWithUTF8String:title.c_str()];
        if (!start_dir.empty())
            panel.directoryURL = [NSURL fileURLWithPath:
                [NSString stringWithUTF8String:start_dir.c_str()]
                                           isDirectory:YES];

        if (!folder && !extensions.empty()) set_extensions(panel, extensions);

        if ([panel runModal] == NSModalResponseOK)
            for (NSURL* url in panel.URLs)
                if (const char* p = url.fileSystemRepresentation) out.push_back(p);
    }
    return out;
}

std::vector<std::string> mac_save(const std::string& title,
                                  const std::vector<std::string>& extensions,
                                  const std::string& start_dir,
                                  const std::string& suggested) {
    @autoreleasepool {
        NSSavePanel* panel = [NSSavePanel savePanel];
        if (!title.empty())
            [panel setTitle:[NSString stringWithUTF8String:title.c_str()]];
        if (!suggested.empty())
            [panel setNameFieldStringValue:
                       [NSString stringWithUTF8String:suggested.c_str()]];
        if (!start_dir.empty())
            [panel setDirectoryURL:
                       [NSURL fileURLWithPath:
                                  [NSString stringWithUTF8String:start_dir.c_str()]
                                   isDirectory:YES]];
        if (!extensions.empty()) set_extensions(panel, extensions);
        std::vector<std::string> out;
        if ([panel runModal] == NSModalResponseOK && [panel URL])
            out.push_back([[[panel URL] path] UTF8String]);
        return out;
    }
}

}  // namespace gui
