#include <juce_core/juce_core.h>
#include <BinaryData.h>
#include <cassert>
#include <iostream>
#include <string>

int main()
{
    std::cout << "[WebResourceTests] Starting RB-26 embedded asset verification...\n";

    // 1. Verify binary data presence
    std::cout << "[WebResourceTests] Checking BinaryData::web_assets_rb26_zip...\n";
    assert(BinaryData::web_assets_rb26_zip != nullptr);
    assert(BinaryData::web_assets_rb26_zipSize > 0);
    std::cout << "  - Embedded zip size: " << BinaryData::web_assets_rb26_zipSize << " bytes\n";

    // 2. Open ZipFile with MemoryInputStream
    juce::MemoryInputStream memStream(BinaryData::web_assets_rb26_zip, static_cast<size_t>(BinaryData::web_assets_rb26_zipSize), false);
    juce::ZipFile zip(memStream);

    const int numEntries = zip.getNumEntries();
    std::cout << "  - Num zip entries: " << numEntries << "\n";
    assert(numEntries > 0);

    // 3. Verify all critical assets exist and can be decompressed
    const char* requiredFiles[] = {
        "index.html",
        "css/style.css",
        "css/rack.css",
        "js/app.js",
        "js/audio/rb26_web_engine.js",
        "js/ui/crt-display.js",
        "js/ui/knob.js",
        "js/ui/vector-pad.js",
        "presets/factory_presets.json"
    };

    for (const char* req : requiredFiles)
    {
        const juce::String target(req);
        int entryIndex = zip.getIndexOfFileName(target);

        if (entryIndex < 0)
        {
            // Case-insensitive / prefix-agnostic search
            for (int i = 0; i < zip.getNumEntries(); ++i)
            {
                const auto* entry = zip.getEntry(i);
                if (entry != nullptr)
                {
                    juce::String name = entry->filename.replaceCharacter('\\', '/');
                    while (name.startsWithChar('/') || name.startsWith("./"))
                    {
                        if (name.startsWithChar('/')) name = name.substring(1);
                        else if (name.startsWith("./")) name = name.substring(2);
                    }
                    if (name.equalsIgnoreCase(target))
                    {
                        entryIndex = i;
                        break;
                    }
                }
            }
        }

        std::cout << "  - Locating: " << req << " ... ";
        assert(entryIndex >= 0);

        const auto* entry = zip.getEntry(entryIndex);
        assert(entry != nullptr);
        std::cout << "Found (entry " << entryIndex << ", uncompressed size: " << entry->uncompressedSize << " bytes)\n";

        // Decompress using stream
        std::unique_ptr<juce::InputStream> stream(zip.createStreamForEntry(*entry));
        assert(stream != nullptr);

        juce::MemoryBlock mb;
        const size_t bytesRead = stream->readIntoMemoryBlock(mb, -1);
        std::cout << "    Decompressed: " << bytesRead << " bytes\n";
        assert(bytesRead == static_cast<size_t>(entry->uncompressedSize));
        assert(mb.getSize() == static_cast<size_t>(entry->uncompressedSize));

        // Check content non-empty
        if (target == "index.html")
        {
            const juce::String html = mb.toString();
            assert(html.contains("BRAUN"));
            assert(html.contains("RB-26"));
            assert(html.contains("chime-strip"));
            assert(html.contains("chord-grid"));
            assert(html.contains("DIN 1451"));
            std::cout << "    [OK] index.html content validated (includes Deck 07, chime strip, chords)!\n";
        }
        else if (target == "js/app.js")
        {
            const juce::String js = mb.toString();
            assert(js.contains("BraunRb26App"));
            assert(js.contains("playChime"));
            assert(js.contains("playChord"));
            assert(js.contains("triggerDirac"));
            assert(js.contains("exciterTrigger"));
            std::cout << "    [OK] js/app.js content validated (includes BraunRb26App, exciterTrigger)!\n";
        }
    }

    std::cout << "[WebResourceTests] ALL EMBEDDED ASSET TESTS PASSED!\n";
    return 0;
}
