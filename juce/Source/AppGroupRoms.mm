#include <JuceHeader.h>
#import <Foundation/Foundation.h>
#include "roms_dir.h"

// The ROM folder in the App Group container, shared with the iOS and macOS apps.
juce::File appGroupRomDirectory()
{
    @autoreleasepool {
        NSURL* url = [[NSFileManager defaultManager] containerURLForSecurityApplicationGroupIdentifier:@"group.tokyo.studio-r.synths"];
        if (url != nil)
            return juce::File (juce::String::fromUTF8 ([[url path] UTF8String])).getChildFile ("TX2000W/roms");
    }
    return {};
}

#if JUCE_IOS
namespace juce { void* getURLBookmark (URL&); }

// 選んだフォルダーは許可の付いた URL でしか読めないので、その間にアプリの中へコピーする。
// iCloud のファイルは中身が端末にないことがあるので、協調読み取りでダウンロードを待つ。時間がかかるので音や画面のスレッドでは呼ばない
static bool copyRomFileFrom (NSURL* root, const smu2000::fs::path& from, const smu2000::fs::path& file, const smu2000::fs::path& to, std::string& why)
{
    NSURL* source = [root URLByAppendingPathComponent: [NSString stringWithUTF8String: from.string().c_str()]];
    [[NSFileManager defaultManager] startDownloadingUbiquitousItemAtURL: source error: nil];
    NSFileCoordinator* coordinator = [[[NSFileCoordinator alloc] initWithFilePresenter: nil] autorelease];
    NSError* failure = nil;
    __block bool copied = false;
    __block std::string reason;
    [coordinator coordinateReadingItemAtURL: source options: 0 error: &failure byAccessor: ^(NSURL* readable)
    {
        copied = smu2000::detail::copy_file (smu2000::fs::path ([readable path].UTF8String), to / file, reason);
    }];
    why = failure != nil ? std::string ([failure localizedDescription].UTF8String) : reason;
    return copied;
}

// dump/ や standin/ に分けず、1 つのフォルダーに全部置いてあってもよい
static bool copyRomFile (NSURL* root, const smu2000::fs::path& file, const smu2000::fs::path& to, std::string& why)
{
    return copyRomFileFrom (root, file, file, to, why) || copyRomFileFrom (root, file.filename(), file, to, why);
}

bool copyRomFolder (juce::URL folder, const juce::File& destination, juce::String& error)
{
    @autoreleasepool {
        BOOL stale = NO;
        auto* bookmark = (NSData*) juce::getURLBookmark (folder);
        NSURL* source = bookmark != nil ? [NSURL URLByResolvingBookmarkData: bookmark options: 0 relativeToURL: nil
                                                        bookmarkDataIsStale: &stale error: nil] : nil;
        if (source == nil)
        {
            error = "Cannot open the folder";
            return false;
        }
        const bool scoped = [source startAccessingSecurityScopedResource];
        const smu2000::fs::path to (destination.getFullPathName().toStdString());
        bool ok = true;
        std::string why;
        for (const auto& file : smu2000::roms_required())
        {
            if (copyRomFile (source, file, to, why))
                continue;
            error = "Cannot read " + juce::String::fromUTF8 (file.string().c_str()) + " in this folder.";
            ok = false;
            break;
        }
        if (ok)
            for (const auto& file : smu2000::roms_optional())
                copyRomFile (source, file, to, why); // なくてもよい
        if (scoped)
            [source stopAccessingSecurityScopedResource];
        return ok;
    }
}
#endif
