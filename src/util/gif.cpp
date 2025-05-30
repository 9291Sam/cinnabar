

#include "util/util.hpp"

//// BEGIN gif_read.hpp

//
//  Gif_read.h
//  gif_read
//
//  Created by Kyle Halladay on 3/31/19.
//  Copyright © 2019 Kyle Halladay. All rights reserved.
//

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"

#include <stdint.h> //if you hate stdint, replace this and the typedefs below with your own integer types

// does not support interlaced or sorted gifs. Debug mode will assert if it encounters a gif like this,
// all asserts are disabled in release mode, so it's recommended that you run at least once in debug to
// verify your gifs are compatible
namespace gif_read
{
    typedef uint16_t uint16;
    typedef uint32_t uint32;
    typedef uint8_t  uint8;
    typedef int32_t  int32;

    // make sure any replacement types are still the right size
    static_assert(sizeof(uint16) == 2, "uint16 type is an incorrect size");
    static_assert(sizeof(uint32) == 4, "uint32 type is an incorrect size");
    static_assert(sizeof(uint8) == 1, "uint8 type is an incorrect size");
    static_assert(sizeof(int32) == 4, "int32 type is an incorrect size");

    // memory heavy GIF class that provides access to any frame of a GIF in arbitrary order
    // keeps a uint8 rgb array of every frame in memory all the time, giving the fastest access to
    // data at runtime, at a large memory cost.
    class GIF
    {
    public:
        // gifFileData is the binary contents of a .gif file. Ctor will memcpy
        // out of this data, but doesn't need it after the ctor finishes.
        // dealloc the gifFileData ptr yourself after constructing a GIF
        GIF(const uint8* gifFileData);
        ~GIF();

        uint32 getWidth() const;
        uint32 getHeight() const;
        uint32 getNumFrames() const;

        // returns an array of unsigned byte RGBA pixel data for a texture with the dimensions
        // defined by the getWidth() and getHeight() function calls. Alpha will always be 255
        const uint8* getFrame(uint32 frameIndex) const;
        const uint8* getFrameAtTime(f32 time, bool looping = true) const;

    private:
        struct GIFImpl* _impl = nullptr;
    };

    // Instead of storing index streams, only the compressed gif data is stored, and is decompressed as new frames are
    // needed To support multiple instances of a GIF displaying different frames, StreamingGIFs are accessed through
    // gif-erators (my stupid name for gif iterators), which require an allocation of 1 frame of gif data each. The
    // memory for all giferators is handled by the streamingGIF class they're allocated by, and accessed through a
    // uint32
    // handle.
    class StreamingGIF
    {
    public:
        // gifFileData is the binary contents of a .gif file. Ctor will memcpy
        // out of this data, but doesn't need it after the ctor finishes.
        // dealloc the gifFileData ptr yourself after construction
        StreamingGIF(const uint8* gifFileData, uint32 maxIterators = 8);
        ~StreamingGIF();
        StreamingGIF(const StreamingGIF&)             = delete;
        StreamingGIF& operator= (const StreamingGIF&) = delete;

        uint32 getWidth() const;
        uint32 getHeight() const;
        uint32 getNumFrames() const;
        f32    getDurationInSeconds() const;

        uint32 createIterator();
        bool   isIteratorValid(uint32 iterator);
        void   destroyIterator(uint32 iterator);

        // returns true if time has advanced enough to get a new frame
        bool tickSingleIterator(uint32 interator, f32 deltaTime);
        void tick(f32 deltaTime); // ticks all iterators

        const uint8* getFirstFrame() const;
        const uint8* getCurrentFrame(uint32 interator) const;

    protected:
        struct StreamingGIFImpl* _impl = nullptr;
    };
} // namespace gif_read

//// END gif_read.hpp

/// BEGIN gif_read.cpp

//
//  gif_read.cpp
//  gif_read
//
//  Created by Kyle Halladay on 3/31/19.
//  Copyright © 2019 Kyle Halladay. All rights reserved.
//

// gif file format information from https://www.fileformat.info/format/gif/egff.htm
// and http://giflib.sourceforge.net/whatsinagif/bits_and_bytes.html
// example gif parser implementation: https://commandlinefanatic.com/cgi-bin/showarticle.cgi?article=art011

#include <cstring>  //for memcpy
#include <stdlib.h> //for malloc, calloc, realloc, free, and exit

#define GT_MALLOC  malloc
#define GT_REALLOC realloc
#define GT_CALLOC  calloc
#define GT_FREE    free

#ifdef DEBUG
#include <stdio.h>
#define GT_CHECK(expr, format, ...)                                                                                    \
    if (!(expr))                                                                                                       \
    {                                                                                                                  \
        fprintf(stdout, "[GIFTEXTURE ERROR]: %s:%d:%s " format "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__);     \
        asm("int $3");                                                                                                 \
        exit(-1);                                                                                                      \
    }
#else
#define GT_CHECK(expr, format, ...)
#endif

#if __clang__
#define GT_PACKED __attribute__((packed))
#else
#error "GT_PACKED not defined for this compiler"
#endif

namespace gif_read
{
    enum BlockType
    {
        BT_Extension       = 0x21,
        BT_ImageDescriptor = 0x2C,
        BT_Trailer         = 0x3B
    };

    enum ExtensionType
    {
        ET_GraphicsControl    = 0xF9,
        ET_ApplicationControl = 0xFF,
        ET_Comment            = 0xFE,
        ET_PlainText          = 0x21
    };

    enum DisposalMethod
    {
        DM_NONE                      = 0,
        DM_KEEP                      = 1,
        DM_CLEAR_TO_BACKGROUND       = 2,
        DM_RESTORE_TO_PREVIOUS_FRAME = 3,
        DM_UNDEFINED                 = 4
    };

    struct IndexStream
    {
        uint16* indices    = nullptr;
        uint32  numIndices = 0;
    };

    const uint32 MAX_GIF_FRAMES     = 8192;
    const uint32 MAX_CODETABLE_ROWS = 4096;
    const uint32 NO_INDEX           = 9999;
    const uint32 NO_BYTE            = 9999;
    const uint16 NO_CODE            = 9999;

    static_assert(NO_INDEX > MAX_CODETABLE_ROWS, "NO_INDEX must be greater than MAX_CODETABLE_ROWS");
    static_assert(NO_CODE > MAX_CODETABLE_ROWS, "NO_CODE needs to be larger than MAX_CODETABLE_ROWS");

    struct GT_PACKED Color
    {
        uint8 rgb[3];
    };
    static_assert(sizeof(Color) == 3, "Color type is an incorrect size. might need to be packed");

    struct GT_PACKED ScreenDescPacked
    {
        uint8 colorTableSize      : 3;
        uint8 sortFlag            : 1;
        uint8 colorResolution     : 3; // max number of entries in a color table - this is NOT bit depth.
        uint8 hasGlobalColorTable : 1;
    };
    static_assert(sizeof(ScreenDescPacked) == 1, "ScreenDescPacked is the incorrect size, needs to be packed");

    struct GT_PACKED Header // need to specify packed here so we can use the size of this struct as a byte offset
    {
        char             signature[3]; // the chars G I F
        char             version[3];   // the chars 8 9 a
        uint16           width;
        uint16           height;
        ScreenDescPacked screenDescriptor;
        uint8            bgColor;     // can be 0 if there is no global color table - likely ignored for us
        uint8            aspectRatio; // ignored for our purposes
    };
    static_assert(sizeof(Header) == 13, "Header is an incorrect size, needs to be packed");

    struct GT_PACKED ImageDescriptor
    {
        uint16 xPos; //(0,0) is top left
        uint16 yPos;
        uint16 width;
        uint16 height;
        uint8  localColorTableFlag : 1;
        uint8  interlaceFlag       : 1;
        uint8  sortFlag            : 1;
        uint8  reserved            : 2;
        uint8  colorTableSize      : 3;
    };
    static_assert(sizeof(ImageDescriptor) == 9, "ImageDescriptor is an incorrect size, needs to be packed");

    struct CodeTableRow
    {
        uint16 byte = NO_BYTE;
        uint16 prev = NO_CODE;
    };

    struct LZWCodeTable
    {
        uint16       codeSize;
        uint16       numCodes;
        CodeTableRow rows[MAX_CODETABLE_ROWS];
    };

    struct Frame
    {
        ImageDescriptor imageDesc;
        Color*          localColorTable; // if null, use global color table
        uint16          lzwMinCodeSize;
    };

    struct GraphicsControlBlock
    {
        uint16         delayTime;
        DisposalMethod disposal;
        uint8          transparentColorIdx;
        uint8          transparentFlag : 1;
    };

    struct DecompressionState
    {
        uint16 partialCode       = NO_CODE;
        uint16 prevCode          = NO_CODE;
        uint16 mask              = 0x01;
        uint16 bitsCurrentlyRead = 0;
    };

    // data common to all gifImpls
    struct GifFileData
    {
        Header               header           = {0};
        Color*               globalColorTable = nullptr;
        uint32               numFrames        = 0;
        uint32               numGfxBlocks     = 0;
        GraphicsControlBlock gfxControlBlocks[MAX_GIF_FRAMES];
        uint32               totalRunTime;
        Frame                imageData[MAX_GIF_FRAMES];
    };

#pragma mark - GIF parsing functions
    void InitializeCodeTable(LZWCodeTable& table, uint16 colorTableLen, uint16 lzwMinCodeSize)
    {
        uint16 numColors = 1 << (colorTableLen + 1);

        table.codeSize = lzwMinCodeSize;
        table.numCodes = (1 << table.codeSize) + 2;

        for (uint32 i = 0; i < MAX_CODETABLE_ROWS; ++i)
        {
            CodeTableRow& row = table.rows[i];
            row.byte          = i < numColors ? i : NO_BYTE;
            row.prev          = NO_INDEX;
        }
    }

    // returns stored part of code in case a single code spans between multiple sub blocks
    DecompressionState compressedDataToIndexStream(
        const uint8*        compressedData,
        uint16              sizeOfCompressedData,
        uint8               colorTableSize,
        uint16              lzwMinCodeSize,
        LZWCodeTable&       codeTable,
        DecompressionState& prevState,
        IndexStream&        outputStream)
    {
        DecompressionState state;
        state.prevCode = prevState.prevCode != NO_CODE ? prevState.prevCode : NO_CODE;
        state.mask     = prevState.partialCode != NO_CODE ? prevState.mask : 0x01;

        uint16& mask      = state.mask; // faster to get a reference to this once than to keep accessing state.mask
        uint16  clearCode = 1 << lzwMinCodeSize;
        uint16  eofCode   = clearCode + 1;

        const uint8* blockData = compressedData;
        uint32       bytesRead = 0;

        while (bytesRead < sizeOfCompressedData)
        {
            uint16 curCode        = prevState.partialCode != NO_CODE ? prevState.partialCode : 0;
            uint16 startingBit    = prevState.partialCode != NO_CODE ? prevState.bitsCurrentlyRead : 0;
            prevState.partialCode = NO_CODE;

            for (uint16 i = startingBit; i < codeTable.codeSize + 1; ++i)
            {
                uint16 bit = ((*blockData) & mask) ? 1 : 0;
                mask       = mask << 1;

                if (mask == 0x100)
                {
                    mask = 0x01;
                    bytesRead++;
                    blockData++;
                }

                curCode = curCode | (bit << i);
                if (bytesRead == sizeOfCompressedData)
                {
                    state.partialCode       = curCode;
                    state.bitsCurrentlyRead = i + 1;
                    return state;
                }
            }

            if (curCode == clearCode)
            {
                InitializeCodeTable(codeTable, colorTableSize, lzwMinCodeSize);
                state.prevCode = NO_CODE;
                continue;
            }
            else if (curCode == eofCode)
            {
                break;
            }
            else if (state.prevCode != NO_CODE && codeTable.codeSize < 12)
            {
                GT_CHECK(
                    curCode <= codeTable.numCodes,
                    "Error parsing compressed data for an image data sub block. Got code %i, but the code table is "
                    "size %i, which means the next new code should have been %i.",
                    curCode,
                    codeTable.numCodes,
                    codeTable.numCodes);

                uint16 codePtr = NO_CODE;

                if (curCode == codeTable.numCodes)
                {
                    codePtr = state.prevCode;
                }
                else
                {
                    codePtr = curCode;
                }

                while (codeTable.rows[codePtr].prev != NO_INDEX)
                {
                    codePtr = codeTable.rows[codePtr].prev;
                }

                CodeTableRow& newRow = codeTable.rows[codeTable.numCodes];
                newRow.byte          = codeTable.rows[codePtr].byte;
                newRow.prev          = state.prevCode;
                codeTable.numCodes++;

                // increase code size if we've filled so many slots in the table that the next index
                // needs more bytes in order to reach a higher slot
                if (codeTable.numCodes == (1 << (codeTable.codeSize + 1)) && codeTable.codeSize < 11)
                {
                    codeTable.codeSize++;
                }
            }

            state.prevCode = curCode;

            uint16 codes[1024];
            uint32 numCodes = 0;
            while (curCode != NO_CODE)
            {
                GT_CHECK(
                    numCodes < 1024,
                    "Error parsing compressed gif data. A code refers to more than 1024 color values but our buffer's "
                    "size is 1024");

                CodeTableRow& curRow = codeTable.rows[curCode];
                GT_CHECK(
                    curRow.prev != curCode,
                    "Error parsing compressed gif data. A codetable row's prevCode value points to itself");

                codes[numCodes++] = curRow.byte;
                curCode           = curRow.prev;
            }

            if (numCodes > 0)
            {
                for (int32 b = numCodes - 1; b > -1; b--)
                {
                    outputStream.indices[outputStream.numIndices++] = codes[b];
                }
            }
        }

        return state;
    }

    void indexStreamToColorArray(
        const IndexStream& indexStream,
        const Color*       colorTable,
        uint8*             outputArray,
        uint32             transparentColorIdx,
        const Frame&       frame,
        const Header&      header)
    {
        uint32 w            = header.width;
        uint32 bytesWritten = 0;
        uint32 nextIdx      = 0;

        // frame rect
        uint32 frameMinX = frame.imageDesc.xPos;
        uint32 frameMinY = frame.imageDesc.yPos;
        uint32 frameMaxX = frameMinX + frame.imageDesc.width;
        uint32 frameMaxY = frameMinY + frame.imageDesc.height;

        uint32 startPixel = frameMinY * w + frameMinX;
        uint32 endPixel   = frameMaxY * w + frameMaxX;
        for (int i = startPixel; i < endPixel; ++i)
        {
            uint32 thisY = i / w;
            uint32 thisX = i - (thisY * w);

            bool isInFrame = thisX >= frameMinX && thisX < frameMaxX && thisY >= frameMinY && thisY < frameMaxY;

            if (isInFrame)
            {
                uint32 code = indexStream.indices[nextIdx++];
                if (code != transparentColorIdx)
                {
                    const uint8* col       = colorTable[code].rgb;
                    outputArray[i * 4]     = col[0];
                    outputArray[i * 4 + 1] = col[1];
                    outputArray[i * 4 + 2] = col[2];
                    outputArray[i * 4 + 3] = 255;
                }
                bytesWritten++;
            }
        }

        //  GT_CHECK(bytesWritten == indexStream.numIndices, "Didn't write all bytes to color array");
    }

    const uint8* parseHeader(const uint8* dataPtr, Header& header)
    {
        memcpy(&header, dataPtr, sizeof(Header));
        dataPtr += sizeof(Header);
        return dataPtr;
    }

    const uint8* parseGlobalColorTable(const uint8* dataPtr, Color** colorTable, const Header& header)
    {
        if (header.screenDescriptor.hasGlobalColorTable)
        {
            uint16 numEntries = 1 << (header.screenDescriptor.colorTableSize + 1);
            *colorTable       = (Color*)GT_MALLOC(sizeof(Color) * numEntries);
            memcpy(*colorTable, dataPtr, sizeof(Color) * numEntries);
            dataPtr += sizeof(Color) * numEntries;
        }

        return dataPtr;
    }

    const uint8* parseExtension(
        const uint8* dataPtr, uint32& totalRunTime, GraphicsControlBlock* gfxControlBlocks, uint32& numGfxBlocks)
    {
        uint8 extensionType  = *dataPtr++;
        uint8 blockSizeBytes = *dataPtr++;

        switch (extensionType)
        {
        case ET_GraphicsControl: {
            GraphicsControlBlock block;
            uint8                packedData = *dataPtr++;
            block.transparentFlag           = (0x1 & packedData);
            block.disposal                  = (DisposalMethod)(packedData >> 2);
            GT_CHECK(block.disposal != DM_RESTORE_TO_PREVIOUS_FRAME, "Restore clear mode is unsupported.");
            GT_CHECK(
                block.disposal < DM_UNDEFINED, "Unknown or unhandled block disposal method in GraphicsControlBlock");

            block.delayTime = *dataPtr;
            totalRunTime += block.delayTime;
            dataPtr += 2;
            block.transparentColorIdx        = *dataPtr++;
            gfxControlBlocks[numGfxBlocks++] = block;
        }
        break;
        case ET_ApplicationControl: {
            // skip over this block because we don't care about whether the gif should loop or not,
            // that will be controlled by application
            dataPtr += blockSizeBytes; // block size only counts the bytes of the NETSCAPE2.0 string
            uint8 subBlockSize = *dataPtr++;
            while (subBlockSize > 0)
            {
                dataPtr += subBlockSize;
                subBlockSize = *dataPtr;
                if (subBlockSize > 0)
                {
                    dataPtr++;
                }
            }
        }
        break;
        case ET_Comment: {
            // ignore comment block
            uint8 subBlockSize = blockSizeBytes;
            while (subBlockSize > 0)
            {
                subBlockSize = *(dataPtr += subBlockSize);
                if (subBlockSize > 0)
                {
                    dataPtr++;
                }
            }
        }
        break;
        case ET_PlainText: {
            // ignore plaintext
            dataPtr += blockSizeBytes;
            uint8 subBlockSize = *dataPtr++;
            while (subBlockSize > 0)
            {
                subBlockSize = *(dataPtr += subBlockSize);
                if (subBlockSize > 0)
                {
                    dataPtr++;
                }
            }
        }
        break;
        default: {
            GT_CHECK(false, "Got an unknown or invalid extension block type: %i\n", extensionType);
        }
        break;
        }

        GT_CHECK(*dataPtr == 0x0, "Error parsing extension block, last byte wasn't 0x0");
        dataPtr++;

        return dataPtr;
    }

    const uint8* parseFrameHeader(const uint8* dataPtr, Frame& outFrame)
    {
        // last byte is a packed uint8 field that needs to be parsed manually
        memcpy(&outFrame.imageDesc, dataPtr, sizeof(ImageDescriptor) - sizeof(uint8));
        dataPtr += sizeof(ImageDescriptor) - sizeof(uint8);

        // had a bug where this was getting read in backwards (yay endianness) so now I'm doing it by hand
        // this shouldn't be necessary... and either this is useless and I just should re-order struct members,
        // or the other place where code is memcpying a packed buffer should be changed to this style... but
        // things appear to be working fine as is so I'm going to leave them until something breaks;
        outFrame.imageDesc.colorTableSize      = *dataPtr & 0x07;       // 0000 0XXX
        outFrame.imageDesc.sortFlag            = (*dataPtr & 0x20) > 0; // 00X0 0000
        outFrame.imageDesc.interlaceFlag       = (*dataPtr & 0x40) > 0; // 0X00 0000
        outFrame.imageDesc.localColorTableFlag = (*dataPtr & 0x80) > 0; // X000 0000
        dataPtr++;

        GT_CHECK(
            outFrame.imageDesc.interlaceFlag == 0, "Got interlaced gif - decoder does not support interlaced gifs");
        GT_CHECK(outFrame.imageDesc.sortFlag == 0, "Got sorted gif - decoder does not support sorted gifs");

        if (outFrame.imageDesc.localColorTableFlag)
        {
            uint16 numEntries        = 1 << (outFrame.imageDesc.colorTableSize + 1);
            outFrame.localColorTable = (Color*)GT_MALLOC(sizeof(Color) * numEntries);
            memcpy(outFrame.localColorTable, dataPtr, sizeof(Color) * numEntries);
            dataPtr += sizeof(Color) * numEntries;
        }
        return dataPtr;
    }

    void filBufferWithBackgroundColor(uint8* frameBuffer, const GifFileData& gif)
    {
        const uint8* bgCol = gif.globalColorTable[gif.header.bgColor].rgb;

        for (int i = 0; i < gif.header.width * gif.header.height; i++)
        {
            frameBuffer[i * 4]     = bgCol[0];
            frameBuffer[i * 4 + 1] = bgCol[1];
            frameBuffer[i * 4 + 2] = bgCol[2];
            frameBuffer[i * 4 + 3] = 255;
        }
    }

    // parses frame, but returns concatenated compressed data instead of decompressing it here
    const uint8* parseFrameNoDecompress(const uint8* dataPtr, GifFileData& gif, uint8** outData, uint16* outSizes)
    {
        Frame   nextFrame = {0};
        uint32& frameIdx  = gif.numFrames;
        GT_CHECK(
            frameIdx < MAX_GIF_FRAMES,
            "Gif has > 4096 frames, but the parsing code's frame buffer only holds 4096. Increase the size of the "
            "parsing code's frame array, or change the code to use a dynamically allocated array to fix.");

        dataPtr                  = parseFrameHeader(dataPtr, nextFrame);
        nextFrame.lzwMinCodeSize = *dataPtr++;
        GT_CHECK(
            nextFrame.lzwMinCodeSize <= 12,
            "Error getting LZWMinCodeSize: value should always be <=12, but current value is %i",
            nextFrame.lzwMinCodeSize);

        // first, iterate over all subblocks to get total size

        const uint8* subBlockIterPtr = dataPtr;

        uint8  sizeOfSubBlock      = *subBlockIterPtr++;
        uint32 totalSizeOfAllCodes = 0; // needs to be 32 bit or else it will overflow on larger gifs
        while (sizeOfSubBlock > 0)
        {
            totalSizeOfAllCodes += sizeOfSubBlock;

            subBlockIterPtr += sizeOfSubBlock;
            sizeOfSubBlock = *subBlockIterPtr;
            if (sizeOfSubBlock > 0)
            {
                subBlockIterPtr++;
            }
        }

        outSizes[frameIdx]      = totalSizeOfAllCodes;
        outData[frameIdx]       = (uint8*)GT_MALLOC(totalSizeOfAllCodes);
        uint32 totalCopiedBytes = 0;

        // then iterate over all subblocks again to get compressed data, now that we've allocated the buffer to hold it

        sizeOfSubBlock = *dataPtr++;
        while (sizeOfSubBlock > 0)
        {
            memcpy(outData[frameIdx] + totalCopiedBytes, dataPtr, sizeOfSubBlock);
            totalCopiedBytes += sizeOfSubBlock;
            dataPtr += sizeOfSubBlock;
            sizeOfSubBlock = *dataPtr;
            if (sizeOfSubBlock > 0)
            {
                dataPtr++;
            }
        }

        gif.imageData[frameIdx] = nextFrame;

        GT_CHECK(*dataPtr == 0x0, "Last byte of frame isn't block terminator. Something has gone wrong");

        dataPtr++;
        frameIdx++;
        return dataPtr;
    }

    // if outimages is null, this function will not convert the frame's index stream to a color array
    const uint8*
    parseFrame(const uint8* dataPtr, uint8* frameBuffer, GifFileData& gif, uint8** outImages, IndexStream* outStream)
    {
        Frame   nextFrame = {0};
        uint32& frameIdx  = gif.numFrames;
        GT_CHECK(
            frameIdx < MAX_GIF_FRAMES,
            "Gif has > 4096 frames, but the parsing code's frame buffer only holds 4096. Increase the size of the "
            "parsing code's frame array, or change the code to use a dynamically allocated array to fix.");

        dataPtr = parseFrameHeader(dataPtr, nextFrame);

        if (gif.numGfxBlocks > 0)
        {
            if (gif.gfxControlBlocks[gif.numGfxBlocks - 1].disposal == DM_CLEAR_TO_BACKGROUND)
            {
                filBufferWithBackgroundColor(frameBuffer, gif);
            }
        }

        nextFrame.lzwMinCodeSize = *dataPtr++;
        GT_CHECK(
            nextFrame.lzwMinCodeSize <= 12,
            "Error getting LZWMinCodeSize: value should always be <=12, but current value is %i",
            nextFrame.lzwMinCodeSize);
        uint8 sizeOfSubBlock = *dataPtr++;

        DecompressionState dcState;

        IndexStream  localFrameStream;
        IndexStream& indexStream = outStream ? *outStream : localFrameStream;
        indexStream.numIndices   = 0;
        indexStream.indices      = (uint16*)GT_MALLOC(sizeof(uint16) * gif.header.width * gif.header.height);

        LZWCodeTable codeTable;
        InitializeCodeTable(
            codeTable,
            nextFrame.imageDesc.localColorTableFlag ? nextFrame.imageDesc.colorTableSize
                                                    : gif.header.screenDescriptor.colorTableSize,
            nextFrame.lzwMinCodeSize);

        while (sizeOfSubBlock > 0)
        {
            dcState = compressedDataToIndexStream(
                dataPtr,
                sizeOfSubBlock,
                nextFrame.imageDesc.localColorTableFlag ? nextFrame.imageDesc.colorTableSize
                                                        : gif.header.screenDescriptor.colorTableSize,
                nextFrame.lzwMinCodeSize,
                codeTable,
                dcState,
                indexStream);

            dataPtr += sizeOfSubBlock;
            sizeOfSubBlock = *dataPtr;
            if (sizeOfSubBlock > 0)
            {
                dataPtr++;
            }
        }

        if (outImages != nullptr)
        {
            uint32 frameSizeBytes = sizeof(uint8) * 4 * gif.header.width * gif.header.height;
            outImages[frameIdx]   = (uint8*)GT_MALLOC(frameSizeBytes);

            uint32 transparentIdx = gif.numGfxBlocks > 0 ? gif.gfxControlBlocks[frameIdx].transparentColorIdx : NO_CODE;
            indexStreamToColorArray(
                indexStream,
                nextFrame.localColorTable ? nextFrame.localColorTable : gif.globalColorTable,
                frameBuffer,
                transparentIdx,
                nextFrame,
                gif.header);

            memcpy(outImages[frameIdx], frameBuffer, frameSizeBytes);
        }

        gif.imageData[frameIdx] = nextFrame;

        GT_CHECK(*dataPtr == 0x0, "Last byte of frame isn't block terminator. Something has gone wrong");

        if (!outStream)
        {
            GT_FREE(indexStream.indices);
        }

        dataPtr++;
        frameIdx++;
        return dataPtr;
    }
} // namespace gif_read

#pragma mark - GIF class methods
namespace gif_read
{
    struct GIFImpl
    {
        GifFileData file;
        uint8*      images[MAX_GIF_FRAMES];
    };

    GIF::GIF(const uint8* gifData)
    {
        _impl            = (GIFImpl*)GT_CALLOC(1, sizeof(GIFImpl));
        GifFileData& gif = _impl->file;
        gif.numFrames    = 0;

        const uint8* ptr = nullptr;
        ptr              = parseHeader(gifData, gif.header);
        ptr              = parseGlobalColorTable(ptr, &gif.globalColorTable, gif.header);

        // working buffer for frame data
        uint8* frameBuffer = (uint8*)GT_MALLOC(gif.header.width * gif.header.height * 4 * sizeof(uint8));
        uint8  nextBlock   = *ptr++;
        while (nextBlock != BT_Trailer)
        {
            if (nextBlock == BT_Extension)
            {
                ptr = parseExtension(ptr, gif.totalRunTime, gif.gfxControlBlocks, gif.numGfxBlocks);
            }
            else if (nextBlock == BT_ImageDescriptor)
            {
                ptr = parseFrame(
                    ptr,
                    frameBuffer,
                    gif,
                    _impl->images,
                    nullptr); // we want to save all images, but discard index streams
            }
            else
            {
                GT_CHECK(
                    false,
                    "Got bad block format byte. Code expects each block to start with either 0x21, 0x2C or 0x3B");
            }

            nextBlock = *ptr++;
        }

        GT_FREE(frameBuffer);

        for (uint32 i = 0; i < gif.numFrames; ++i)
        {
            if (gif.imageData[i].localColorTable)
            {
                GT_FREE(gif.imageData[i].localColorTable);
            }
        }
    }

    const uint8* GIF::getFrame(uint32 frameIndex) const
    {
        GT_CHECK(frameIndex < _impl->file.numFrames, "Out-of-bounds error when trying to get Gif frame");
        return _impl->images[frameIndex];
    }

    const uint8* GIF::getFrameAtTime(f32 time, bool looping) const
    {
        GT_CHECK(time >= 0, "Attempting to get a gif frame at a negative time (%f)", time);
        GifFileData& gif     = _impl->file;
        uint32       runTime = gif.totalRunTime;
        if (runTime == 0)
        {
            return _impl->images[0];
        }

        uint32 runningTime = 0;
        uint32 hundredths  = looping ? (uint32)(time * 100) % gif.totalRunTime : (time) * 100;

        for (uint32 i = 0; i < gif.numGfxBlocks; ++i)
        {
            runningTime += gif.gfxControlBlocks[i].delayTime;
            if (hundredths <= runningTime)
            {
                return _impl->images[i];
            }
        }

        return _impl->images[gif.numFrames - 1];
    }

    uint32 GIF::getWidth() const
    {
        return _impl->file.header.width;
    }

    uint32 GIF::getHeight() const
    {
        return _impl->file.header.height;
    }

    uint32 GIF::getNumFrames() const
    {
        return _impl->file.numFrames;
    }

    GIF::~GIF()
    {
        if (_impl)
        {
            GT_FREE(_impl->file.globalColorTable);
            for (uint32 i = 0; i < _impl->file.numFrames; ++i)
            {
                GT_FREE(_impl->images[i]);
            }
            GT_FREE(_impl);
        }
    }
} // namespace gif_read

#pragma mark - IStreamingGIF class methods
namespace gif_read
{
    struct StreamingGIFIter
    {
        f32    currentTime     = 0.0f;
        uint16 currentFrameIdx = 0;
        uint8* currentFrame    = nullptr;
    };

    struct StreamingGIFImpl
    {
        GifFileData file;
        uint8*      firstFrame = nullptr;

        IndexStream* indexStreams = nullptr; // streaming gif pre-calculates the index stream for each frame
        uint8** compressedData    = nullptr; // streaming compressed gif pre-concatenates compressed data for each frame
        uint16* compressedDataSizes = nullptr;
        DecompressionState decompressionState;

        StreamingGIFIter* iterators;
        uint32            numIterators;
        uint32            maxIterators;
    };

    const uint8* StreamingGIF::getCurrentFrame(uint32 iterator) const
    {
        GT_CHECK(iterator < _impl->maxIterators, "Attempting to get frame for iterator that does not exist");
        GT_CHECK(iterator < _impl->numIterators, "Attempting to get frame for an iterator that does not exist");

        StreamingGIFIter& iter = _impl->iterators[iterator];
        if (iter.currentFrameIdx == 0)
        {
            return _impl->firstFrame;
        }
        return _impl->iterators[iterator].currentFrame;
    }

    const uint8* StreamingGIF::getFirstFrame() const
    {
        return _impl->firstFrame;
    }

    uint32 StreamingGIF::getWidth() const
    {
        return _impl->file.header.width;
    }

    uint32 StreamingGIF::getHeight() const
    {
        return _impl->file.header.height;
    }

    uint32 StreamingGIF::getNumFrames() const
    {
        return _impl->file.numFrames;
    }

    f32 StreamingGIF::getDurationInSeconds() const
    {
        return _impl->file.totalRunTime * 100;
    }
} // namespace gif_read

#pragma mark - StreamingGIF class methods
namespace gif_read
{
    StreamingGIF::StreamingGIF(const uint8* gifData, uint32 inMaxIterators /* = 8 */)
    {
        _impl               = (StreamingGIFImpl*)GT_CALLOC(1, sizeof(StreamingGIFImpl));
        _impl->iterators    = (StreamingGIFIter*)GT_MALLOC(sizeof(StreamingGIFIter) * inMaxIterators);
        _impl->maxIterators = inMaxIterators;

        GifFileData& gif = _impl->file;
        gif.numFrames    = 0;

        const uint8* ptr = nullptr;
        ptr              = parseHeader(gifData, gif.header);
        ptr              = parseGlobalColorTable(ptr, &gif.globalColorTable, gif.header);

        // working buffer for frame data
        _impl->firstFrame          = (uint8*)GT_MALLOC(gif.header.width * gif.header.height * 4 * sizeof(uint8));
        _impl->compressedData      = (uint8**)GT_MALLOC(sizeof(uint8**) * MAX_GIF_FRAMES);
        _impl->compressedDataSizes = (uint16*)GT_MALLOC(sizeof(uint16) * MAX_GIF_FRAMES);

        uint8 nextBlock = *ptr++;
        while (nextBlock != BT_Trailer)
        {
            if (nextBlock == BT_Extension)
            {
                ptr = parseExtension(ptr, gif.totalRunTime, gif.gfxControlBlocks, gif.numGfxBlocks);
            }
            else if (nextBlock == BT_ImageDescriptor)
            {
                ptr = parseFrameNoDecompress(ptr, gif, _impl->compressedData, _impl->compressedDataSizes);
            }
            else
            {
                GT_CHECK(
                    false,
                    "Got bad block format byte. Code expects each block to start with either 0x21, 0x2C or 0x3B");
            }

            nextBlock = *ptr++;
        }

        Frame&       firstFrame = _impl->file.imageData[0];
        LZWCodeTable codeTable  = {0};
        InitializeCodeTable(
            codeTable,
            firstFrame.imageDesc.localColorTableFlag ? firstFrame.imageDesc.colorTableSize
                                                     : gif.header.screenDescriptor.colorTableSize,
            firstFrame.lzwMinCodeSize);

        _impl->decompressionState = {0};

        _impl->indexStreams               = (IndexStream*)GT_MALLOC(sizeof(IndexStream));
        _impl->indexStreams[0].numIndices = 0;
        _impl->indexStreams[0].indices    = (uint16*)GT_MALLOC(sizeof(uint16) * gif.header.width * gif.header.height);

        compressedDataToIndexStream(
            _impl->compressedData[0],
            _impl->compressedDataSizes[0],
            firstFrame.imageDesc.localColorTableFlag ? firstFrame.imageDesc.colorTableSize
                                                     : gif.header.screenDescriptor.colorTableSize,
            firstFrame.lzwMinCodeSize,
            codeTable,
            _impl->decompressionState,
            _impl->indexStreams[0]);

        uint32 transparentIdx = gif.numGfxBlocks > 0 ? gif.gfxControlBlocks[0].transparentColorIdx : NO_CODE;
        indexStreamToColorArray(
            _impl->indexStreams[0],
            firstFrame.imageDesc.localColorTableFlag ? firstFrame.localColorTable : gif.globalColorTable,
            _impl->firstFrame,
            transparentIdx,
            firstFrame,
            gif.header);
    }

    bool StreamingGIF::tickSingleIterator(uint32 iterator, f32 deltaTime)
    {
        if (!isIteratorValid(iterator))
        {
            return false;
        }
        GT_CHECK(iterator < _impl->maxIterators, "Attempting to tick an iterator that does not exist");
        GT_CHECK(iterator < _impl->numIterators, "Attempting to tick an iterator that does not exist");
        StreamingGIFIter& iter = _impl->iterators[iterator];
        iter.currentTime += deltaTime;

        GifFileData& gif     = _impl->file;
        uint32       runTime = gif.totalRunTime;
        if (runTime == 0)
        {
            return false;
        }

        uint32 runningTime = 0;
        uint32 hundredths  = (uint32)(iter.currentTime * 100.0f) % gif.totalRunTime;

        for (uint32 i = 0; i < gif.numGfxBlocks; ++i)
        {
            runningTime += gif.gfxControlBlocks[i].delayTime;
            if (hundredths < runningTime)
            {
                if (iter.currentFrameIdx != i)
                {
                    if (i == 0)
                    {
                        memcpy(
                            iter.currentFrame,
                            _impl->firstFrame,
                            gif.header.width * gif.header.height * 4 * sizeof(uint8));
                    }
                    else
                    {
                        // next frame
                        Frame& frameData = gif.imageData[i];
                        uint32 transparentIdx =
                            gif.numGfxBlocks > 0 ? gif.gfxControlBlocks[i].transparentColorIdx : NO_CODE;
                        Color* colorTable =
                            frameData.localColorTable ? frameData.localColorTable : gif.globalColorTable;

                        LZWCodeTable codeTable = {0};
                        InitializeCodeTable(
                            codeTable,
                            frameData.imageDesc.localColorTableFlag ? frameData.imageDesc.colorTableSize
                                                                    : gif.header.screenDescriptor.colorTableSize,
                            frameData.lzwMinCodeSize);

                        _impl->indexStreams[0].numIndices = 0;

                        compressedDataToIndexStream(
                            _impl->compressedData[i],
                            _impl->compressedDataSizes[i],
                            frameData.imageDesc.localColorTableFlag ? frameData.imageDesc.colorTableSize
                                                                    : gif.header.screenDescriptor.colorTableSize,
                            frameData.lzwMinCodeSize,
                            codeTable,
                            _impl->decompressionState,
                            _impl->indexStreams[0]);

                        indexStreamToColorArray(
                            _impl->indexStreams[0],
                            colorTable,
                            iter.currentFrame,
                            transparentIdx,
                            frameData,
                            gif.header);
                    }

                    iter.currentFrameIdx = i;
                }
                return true;
            }
        }

        return false;
    }

    // will only ever increment the current frame by 1. If you provide a
    // timestep that requires jumping multiple frames, it will take several
    // calls to tick() to catch up. This is intentional, to prevent CPU hitches
    // from causing atypical CPU usage when encountering a hitch in a
    // running game
    void StreamingGIF::tick(f32 deltaTime)
    {
        GT_CHECK(deltaTime > 0, "Passed negative time to StreamingGif::Tick()");
        deltaTime = deltaTime > 0 ? deltaTime : 0;

        GifFileData& gif     = _impl->file;
        uint32       runTime = gif.totalRunTime;
        if (runTime == 0)
        {
            return;
        }

        for (uint32 i = 0; i < _impl->numIterators; ++i)
        {
            tickSingleIterator(i, deltaTime);
        }
    }

    bool StreamingGIF::isIteratorValid(uint32 iterator)
    {
        if (iterator > _impl->maxIterators)
        {
            return false;
        }
        if (iterator > _impl->numIterators)
        {
            return false;
        }
        if (_impl->iterators[iterator].currentFrame == nullptr)
        {
            return false;
        }
        return true;
    }

    uint32 StreamingGIF::createIterator()
    {
        StreamingGIFIter& iter = _impl->iterators[_impl->numIterators];
        iter.currentFrame      = 0;
        iter.currentTime       = 0;
        iter.currentFrameIdx   = 0;
        GifFileData& gif       = _impl->file;

        iter.currentFrame = (uint8*)GT_MALLOC(gif.header.width * gif.header.height * 4 * sizeof(uint8));
        memcpy(iter.currentFrame, _impl->firstFrame, gif.header.width * gif.header.height * 4 * sizeof(uint8));

        _impl->numIterators++;
        return _impl->numIterators - 1;
    }

    void StreamingGIF::destroyIterator(uint32 iterator)
    {
        if (_impl->iterators[iterator].currentFrame)
        {
            GT_FREE(_impl->iterators[iterator].currentFrame);
        }
        else
        {
            GT_CHECK(iterator < _impl->maxIterators, "Attempting to free an iterator with index > maxIterators");
            GT_CHECK(0, "Attemping to destroy an iterator that has not been created");
        }
    }

    StreamingGIF::~StreamingGIF()
    {
        GT_FREE(_impl->indexStreams[0].indices);
        GT_FREE(_impl->indexStreams);

        for (uint16 i = 0; i < _impl->file.numFrames; ++i)
        {
            GT_FREE(_impl->compressedData[i]);
        }
        GT_FREE(_impl->compressedData);
        GT_FREE(_impl->compressedDataSizes);

        if (_impl)
        {
            GT_FREE(_impl->file.globalColorTable);

            if (_impl->firstFrame)
            {
                GT_FREE(_impl->firstFrame);
                _impl->firstFrame = nullptr;
            }

            for (uint32 i = 0; i < _impl->maxIterators; ++i)
            {
                destroyIterator(i);
            }

            GT_FREE(_impl);
        }
    }
} // namespace gif_read

#undef GT_MALLOC
#undef GT_REALLOC
#undef GT_CALLOC
#undef GT_FREE
#undef GT_CHECK

//// END gif_read.cpp

#pragma clang diagnostic pop

#include "gif.hpp"
#include "util/logger.hpp"
#include <algorithm>
#include <unordered_map>

namespace util
{
    Gif::Gif()
        : total_time {}
    {}
    Gif::Gif(std::span<const std::byte> newFrameData)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        gif_read::GIF gifLoader {reinterpret_cast<const u8*>(newFrameData.data())};

        const u32 numberOfFrames = gifLoader.getNumFrames();

        const usize width  = gifLoader.getWidth();
        const usize height = gifLoader.getHeight();

        this->extent = {static_cast<u32>(width), static_cast<u32>(height)};

        std::unordered_map<const u8*, f32> discoveredFramesToTimesMap;

        static constexpr f32 ProbeTimeGranularity = 0.001f;

        f32 thisProbeTime = 0.0f;
        while (discoveredFramesToTimesMap.size() < numberOfFrames)
        {
            const u8* discoveredFramePointer = gifLoader.getFrameAtTime(thisProbeTime, false);

            if (!discoveredFramesToTimesMap.contains(discoveredFramePointer))
            {
                discoveredFramesToTimesMap.insert({discoveredFramePointer, thisProbeTime});
            }

            thisProbeTime += ProbeTimeGranularity;
        }

        this->total_time = thisProbeTime;

        this->frame_start_times.reserve(discoveredFramesToTimesMap.size());
        for (const auto [_, time] : discoveredFramesToTimesMap)
        {
            this->frame_start_times.push_back(time);
        }
        std::ranges::sort(this->frame_start_times);

        const usize sizeBytesOneFrame = width * height * sizeof(Color);

        for (const f32 frameStartTime : this->frame_start_times)
        {
            const u8* rawFrameData = gifLoader.getFrameAtTime(frameStartTime);

            std::vector<Color> thisFrameStorage {};
            thisFrameStorage.resize(sizeBytesOneFrame);
            std::memcpy(thisFrameStorage.data(), rawFrameData, sizeBytesOneFrame);

            this->frames.push_back(std::move(thisFrameStorage));

            // log::trace("discovered frame start time of {}", frameStartTime);
        }
    }

    std::mdspan<const Gif::Color, std::dextents<usize, 2>> Gif::getFrame(FrameNumber frameNumber) const
    {
        const std::vector<Color>& thisFrame = this->frames.at(frameNumber);

        return std::mdspan(thisFrame.data(), this->extent.first, this->extent.second);
    }

    std::mdspan<const Gif::Color, std::dextents<usize, 2>>
    Gif::getFrameAtTime(f32 time, std::optional<Looping> looping) const
    {
        return this->getFrame(this->getFrameNumberAtTime(time, looping));
    }

    util::Gif::FrameNumber Gif::getFrameNumberAtTime(f32 time, std::optional<Looping> looping) const
    {
        if (looping.has_value())
        {
            time = std::fmod(time, this->total_time);
        }

        const std::vector<f32>::const_iterator it = std::ranges::lower_bound(this->frame_start_times, time);

        return static_cast<FrameNumber>(it - this->frame_start_times.cbegin());
    }

    std::pair<u32, u32> Gif::getExtents() const
    {
        return this->extent;
    }

    f32 Gif::getTotalTime() const
    {
        return this->total_time;
    }

    std::span<const f32> Gif::getAllStartTimes() const
    {
        return std::span<const f32> {this->frame_start_times.data(), this->frame_start_times.size()};
    }

} // namespace util