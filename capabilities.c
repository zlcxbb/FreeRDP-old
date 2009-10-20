/* -*- c-basic-offset: 8 -*-
   FreeRDP: A Remote Desktop Protocol client.
   Protocol services - Capability sets

   Copyright (C) Marc-Andre Moreau <marcandre.moreau@gmail.com> 2009

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "rdesktop.h"
#include "rdp.h"
#include "rdpset.h"
#include "pstcache.h"
#include "capabilities.h"

/* Output capability set header */
void
rdp_out_capset_header(STREAM s, uint16 capabilitySetType, uint16 lengthCapability)
{
	out_uint16_le(s, capabilitySetType); // capabilitySetType
	out_uint16_le(s, lengthCapability); // lengthCapability
}

/* Output general capability set */
void
rdp_out_general_capset(rdpRdp * rdp, STREAM s)
{
	rdp_out_capset_header(s, CAPSET_TYPE_GENERAL, CAPSET_LEN_GENERAL);

	out_uint16_le(s, OS_MAJOR_TYPE_WINDOWS); // osMajorType, should we lie?
	out_uint16_le(s, OS_MINOR_TYPE_WINDOWS_NT); // osMinorType
	out_uint16_le(s, CAPS_PROTOCOL_VERSION); // protocolVersion
	out_uint16(s, 0); // pad
	out_uint16(s, 0); // generalCompressionTypes, must be set to 0

	// This section is not part of RDP 4

	if(rdp->settings->rdp_version >= 5)
	{
		out_uint16_le(s,
			NO_BITMAP_COMPRESSION_HDR |
			LONG_CREDENTIALS_SUPPORTED |
			AUTORECONNECT_SUPPORTED); // extraFlags
		out_uint16(s, 0); // updateCapabilityFlag, must be set to 0
		out_uint16(s, 0); // remoteUnshareFlag, must be set to 0
		out_uint16(s, 0); // generalCompressionLevel, must be set to 0
		out_uint8(s, 0); // refreshRectSupport, either TRUE (0x01) or FALSE (0x00)
		out_uint8(s, 0); // suppressOutputSupport, either TRUE (0x01) or FALSE (0x00)
	}
}

/* Output bitmap capability set */
void
rdp_out_bitmap_capset(rdpRdp * rdp, STREAM s)
{
	rdp_out_capset_header(s, CAPSET_TYPE_BITMAP, CAPSET_LEN_BITMAP);

	/*
	 * preferredBitsPerPixel (2 bytes):
	 * A 16-bit, unsigned integer. Color depth of the remote session. In RDP 4.0 and 5.0,
	 * this field MUST be set to 8 (even for a 16-color session)
	 */

	if(rdp->settings->rdp_version <= 5)
	{
		out_uint16_le(s, 8); // preferredBitsPerPixel
	}
	else
	{
		out_uint16_le(s, rdp->settings->server_depth); // preferredBitsPerPixel
	}
	
	out_uint16_le(s, 1); // receive1BitPerPixel
	out_uint16_le(s, 1); // receive4BitsPerPixel
	out_uint16_le(s, 1); // receive8BitsPerPixel
	out_uint16_le(s, rdp->settings->width); // desktopWidth
	out_uint16_le(s, rdp->settings->height); // desktopHeight
	out_uint16(s, 0); // pad
	out_uint16_le(s, 1); // desktopResizeFlag
	out_uint16_le(s, rdp->settings->bitmap_compression ? 1 : 0); // bitmapCompressionFlag
	out_uint8(s, 0); // highColorFlags, ignored and should be set to zero
	out_uint8(s, 1); // drawingFlags, indicating support for 32 bpp bitmaps
	out_uint16_le(s, 1); // multipleRectangleSupport, must be set to true
	out_uint16(s, 0); // pad
}

/* Process bitmap capability set */
void
rdp_process_bitmap_capset(rdpRdp * rdp, STREAM s)
{
	uint16 preferredBitsPerPixel;
	uint16 desktopWidth;
	uint16 desktopHeight;
	uint16 desktopResizeFlag;
	uint16 bitmapCompressionFlag;
	uint8 drawingFlags;

	/*
	 * preferredBitsPerPixel (2 bytes):
	 * A 16-bit, unsigned integer. Color depth of the remote session. In RDP 4.0 and 5.0,
	 * this field MUST be set to 8 (even for a 16-color session)
	 */

	in_uint16_le(s, preferredBitsPerPixel); // preferredBitsPerPixel
	in_uint8s(s, 6); // Ignore receive1BitPerPixel, receive4BitPerPixel, receive8BitPerPixel
	in_uint16_le(s, desktopWidth); // desktopWidth
	in_uint16_le(s, desktopHeight); // desktopHeight
	in_uint8s(s, 2); // Ignore pad
	in_uint16(s, desktopResizeFlag); // desktopResizeFlag
	in_uint16(s, bitmapCompressionFlag); // bitmapCompressionFlag
	in_uint8s(s, 1); // Ignore highColorFlags
	in_uint8(s, drawingFlags); // drawingFlags

	/*
	 * The server may limit depth and change the size of the desktop (for
	 * example when shadowing another session).
	 */
	if (rdp->settings->server_depth != preferredBitsPerPixel)
	{
		ui_warning(rdp->inst, "Remote desktop does not support colour depth %d; falling back to %d\n",
			rdp->settings->server_depth, preferredBitsPerPixel);
		rdp->settings->server_depth = preferredBitsPerPixel;
	}
	if (rdp->settings->width != desktopWidth || rdp->settings->height != desktopHeight)
	{
		ui_warning(rdp->inst, "Remote desktop changed from %dx%d to %dx%d.\n", rdp->settings->width,
			rdp->settings->height, desktopWidth, desktopHeight);
		rdp->settings->width = desktopWidth;
		rdp->settings->height = desktopHeight;
		ui_resize_window(rdp->inst);
	}
}

/* Output order capability set */
void
rdp_out_order_capset(rdpRdp * rdp, STREAM s)
{
	uint8 orderSupport[32];

	memset(orderSupport, 0, 32);
	orderSupport[NEG_DSTBLT_INDEX] = 1;
	orderSupport[NEG_PATBLT_INDEX] = 1;
	orderSupport[NEG_SCRBLT_INDEX] = 1;
	orderSupport[NEG_MEMBLT_INDEX] = (rdp->settings->bitmap_cache ? 1 : 0);
	orderSupport[NEG_MEM3BLT_INDEX] = (rdp->settings->triblt ? 1 : 0);
	// orderSupport[NEG_DRAWNINEGRID_INDEX] = 1;
	orderSupport[NEG_LINETO_INDEX] = 1;
	orderSupport[NEG_MULTI_DRAWNINEGRID_INDEX] = 1;
	orderSupport[NEG_SAVEBITMAP_INDEX] = (rdp->settings->desktop_save ? 1 : 0);
	// orderSupport[NEG_MULTIDSTBLT_INDEX] = 1;
	// orderSupport[NEG_MULTIPATBLT_INDEX] = 1;
	// orderSupport[NEG_MULTISCRBLT_INDEX] = 1;
	// orderSupport[NEG_MULTIOPAQUERECT_INDEX] = 1;
	// orderSupport[NEG_FAST_INDEX_INDEX] = 1;
	orderSupport[NEG_POLYGON_SC_INDEX] = (rdp->settings->polygon_ellipse_orders ? 1 : 0);
	orderSupport[NEG_POLYGON_CB_INDEX] = (rdp->settings->polygon_ellipse_orders ? 1 : 0);
	orderSupport[NEG_POLYLINE_INDEX] = 1;
	// orderSupport[NEG_FAST_GLYPH_INDEX] = 1;
	orderSupport[NEG_ELLIPSE_SC_INDEX] = (rdp->settings->polygon_ellipse_orders ? 1 : 0);
	orderSupport[NEG_ELLIPSE_CB_INDEX] = (rdp->settings->polygon_ellipse_orders ? 1 : 0);
	orderSupport[NEG_INDEX_INDEX] = 1;

	out_uint16_le(s, CAPSET_TYPE_ORDER);
	out_uint16_le(s, CAPSET_LEN_ORDER);

	out_uint8s(s, 16); // terminalDescriptor, ignored and should all be set to zeros
	out_uint32(s, 0); // pad
	out_uint16_le(s, 1); // desktopSaveXGranularity
	out_uint16_le(s, 20); // desktopSaveYGranularity
	out_uint16(s, 0); // pad
	out_uint16_le(s, 1); // maximumOrderLevel
	out_uint16_le(s, 0); // numberFonts, ignored and should be set to zero
	
	out_uint16_le(s,
		NEGOTIATEORDERSUPPORT |
		ZEROBOUNDSDELTASSUPPORT |
		COLORINDEXSUPPORT ); // orderFlags

	out_uint8p(s, orderSupport, 32); // orderSupport
	out_uint16_le(s, 0); // textFlags, must be ignored
	out_uint16_le(s, 0); // orderSupportExFlags
	out_uint32(s, 0); // pad
	out_uint32_le(s, rdp->settings->desktop_save == False ? 0 : 0x38400); // desktopSaveSize
	out_uint16(s, 0); // pad
	out_uint16(s, 0); // pad

	/* See [MSDN-CP]: http://msdn.microsoft.com/en-us/library/dd317756(VS.85).aspx */
	out_uint16_le(s, 0x04E4); // textANSICodePage, 0x04E4 is "ANSI Latin 1 Western European (Windows)"
	out_uint16(s, 0); // pad
}

/* Process order capability set */
void
rdp_process_order_capset(rdpRdp * rdp, STREAM s)
{
	uint8 orderSupport[32];
	uint16 desktopSaveXGranularity;
	uint16 desktopSaveYGranularity;
	uint16 maximumOrderLevel;
	uint16 orderFlags;
	uint16 orderSupportExFlags;
	uint32 desktopSaveSize;
	uint16 textANSICodePage;
	memset(orderSupport, 0, 32);

	in_uint8s(s, 20); // Ignore terminalDescriptor and pad
	in_uint16_le(s, desktopSaveXGranularity); // desktopSaveXGranularity
	in_uint16_le(s, desktopSaveYGranularity); // desktopSaveYGranularity
	in_uint8s(s, 2); // Ignore pad
	in_uint16_le(s, maximumOrderLevel); // maximumOrderLevel
	in_uint8s(s, 2); // Ignore numberFonts
	in_uint16_le(s, orderFlags); // orderFlags
	in_uint8a(s, orderSupport, 32); // orderSupport
	in_uint8s(s, 2); // Ignore textFlags
	in_uint16_le(s, orderSupportExFlags); // orderSupportExFlags
	in_uint8s(s, 4); // Ignore pad
	in_uint32_le(s, desktopSaveSize); // desktopSaveSize
	in_uint8s(s, 4); // Ignore pad

	/* See [MSDN-CP]: http://msdn.microsoft.com/en-us/library/dd317756(VS.85).aspx */
	in_uint16_le(s, textANSICodePage); // textANSICodePage
	// Ignore pad
}

/* Output bitmap cache capability set */
void
rdp_out_bitmapcache_capset(rdpRdp * rdp, STREAM s)
{
	int Bpp;
	rdp_out_capset_header(s, CAPSET_TYPE_BITMAPCACHE, CAPSET_LEN_BITMAPCACHE);

	Bpp = (rdp->settings->server_depth + 7) / 8;	/* bytes per pixel */
	out_uint8s(s, 24); // pad
	out_uint16_le(s, 0x258); // Cache1Entries
	out_uint16_le(s, 0x100 * Bpp); // Cache1MaximumCellSize
	out_uint16_le(s, 0x12c); // Cache2Entries
	out_uint16_le(s, 0x400 * Bpp); // Cache2MaximumCellSize
	out_uint16_le(s, 0x106); // Cache3Entries
	out_uint16_le(s, 0x1000 * Bpp); //Cache3MaximumCellSize
}

/* Output bitmap cache v2 capability set */
void
rdp_out_bitmapcache_rev2_capset(rdpRdp * rdp, STREAM s)
{
	rdp_out_capset_header(s, CAPSET_TYPE_BITMAPCACHE_REV2, CAPSET_LEN_BITMAPCACHE_REV2);

	out_uint16_le(s, rdp->settings->bitmap_cache_persist_enable ? 2 : 0); // CacheFlags
	out_uint8s(s, 1); // pad
	out_uint8(s, 3); // numCellCaches

	/* max cell size for cache 0 is 16x16, 1 = 32x32, 2 = 64x64, etc */
	out_uint32_le(s, BMPCACHE2_C0_CELLS);
	out_uint32_le(s, BMPCACHE2_C1_CELLS);
	if (pstcache_init(rdp->pcache, 2))
	{
		out_uint32_le(s, BMPCACHE2_NUM_PSTCELLS | BMPCACHE2_FLAG_PERSIST);
	}
	else
	{
		out_uint32_le(s, BMPCACHE2_C2_CELLS);
	}
	out_uint8s(s, 20);	/* other bitmap caches not used */
}

/* Output bitmap cache host support capability set */
void
rdp_out_bitmapcache_hostsupport_capset(rdpRdp * rdp, STREAM s)
{
	rdp_out_capset_header(s, CAPSET_TYPE_BITMAPCACHE_HOSTSUPPORT, CAPSET_LEN_BITMAPCACHE_HOSTSUPPORT);

        out_uint8(s, BITMAPCACHE_REV2); // cacheVersion, must be set to BITMAPCACHE_REV2
        out_uint8(s, 0); // pad
        out_uint16(s, 0); // pad
}

/* Process bitmap cache host support capability set */
void
rdp_process_bitmapcache_hostsupport_capset(rdpRdp * rdp, STREAM s)
{
        uint8 cacheVersion;
        in_uint8(s, cacheVersion); // cacheVersion, must be set to BITMAPCACHE_REV2
        // pad (1 byte)
        // pad (2 bytes)
}

/* Output input capability set */
void
rdp_out_input_capset(rdpRdp * rdp, STREAM s)
{
	rdp_out_capset_header(s, CAPSET_TYPE_INPUT, CAPSET_LEN_INPUT);

	out_uint16_le(s, INPUT_FLAG_SCANCODES | INPUT_FLAG_MOUSEX | INPUT_FLAG_UNICODE); // inputFlags
	out_uint16(s, 0); // pad
        out_uint32_le(s, rdp->settings->keyboard_layout); // keyboardLayout
	out_uint32_le(s, rdp->settings->keyboard_type); // keyboardType
	out_uint32_le(s, rdp->settings->keyboard_subtype); // keyboardSubType
	out_uint32_le(s, rdp->settings->keyboard_functionkeys); // keyboardFunctionKeys

	//rdp_out_unistr(rdp, s, rdp->settings->keyboard_ime, 2 * strlen(rdp->settings->keyboard_ime));
	//out_uint8s(s, 62 - 2 * strlen(rdp->settings->keyboard_ime)); // imeFileName (64 bytes)
	out_uint8s(s, 64);
}

/* Process input capability set */
void
rdp_process_input_capset(rdpRdp * rdp, STREAM s)
{
        uint16 inputFlags;
        uint32 keyboardLayout;
        uint32 keyboardType;
        uint32 keyboardSubType;
        uint32 keyboardFunctionKeys;

	in_uint16_le(s, inputFlags); // inputFlags
	in_uint8s(s, 2); // pad
        in_uint32_le(s, keyboardLayout); // keyboardLayout
	in_uint32_le(s, keyboardType); // keyboardType
	in_uint32_le(s, keyboardSubType); // keyboardSubType
	in_uint32_le(s, keyboardFunctionKeys); // keyboardFunctionKeys
        // in_uint8s(s, 64); // imeFileName (64 bytes)
}

/* Output font capability set */
void
rdp_out_font_capset(STREAM s)
{
	rdp_out_capset_header(s, CAPSET_TYPE_FONT, CAPSET_LEN_FONT);

	out_uint16_le(s, FONTSUPPORT_FONTLIST); // fontSupportFlags
	out_uint16(s, 0); // pad
}

/* Process font capability set */
void
rdp_process_font_capset(rdpRdp * rdp, STREAM s)
{
        uint16 fontSupportFlags;
	in_uint16_le(s, fontSupportFlags); // fontSupportFlags
	// pad (2 bytes)
}

/* Output control capability set */
void
rdp_out_control_capset(STREAM s)
{
	rdp_out_capset_header(s, CAPSET_TYPE_CONTROL, CAPSET_LEN_CONTROL);

	out_uint16(s, 0); // controlFlags, should be set to zero
	out_uint16(s, 0); // remoteDetachFlag, should be set to FALSE
	out_uint16_le(s, CONTROLPRIORITY_NEVER); // controlInterest
	out_uint16_le(s, CONTROLPRIORITY_NEVER); // detachInterest
}

/* Output window activation capability set */
void
rdp_out_window_activation_capset(STREAM s)
{
	rdp_out_capset_header(s, CAPSET_TYPE_ACTIVATION, CAPSET_LEN_ACTIVATION);

	/* All of the following should be set to FALSE (0x0) */
	out_uint16(s, 0); // helpKeyFlag
	out_uint16(s, 0); // helpKeyIndexFlag
	out_uint16(s, 0); // helpExtendedKeyFlag
	out_uint16(s, 0); // windowManagerKeyFlag
}

/* Output pointer capability set */
void
rdp_out_pointer_capset(rdpRdp * rdp, STREAM s)
{
	int len;

	len = CAPSET_LEN_POINTER;
	if (rdp->settings->new_cursors)
	{
		len += 2;
	}
	rdp_out_capset_header(s, CAPSET_TYPE_POINTER, len);
	out_uint16_le(s, 1); /* colorPointerFlag (assumed to be always true) */
	out_uint16_le(s, 20); /* colorPointerCacheSize */
	if (rdp->settings->new_cursors)
	{
		/*
		* pointerCacheSize (2 bytes):
		* Optional, if absent or set to 0 the server
		* will not use the New Pointer Update
		*/
		out_uint16_le(s, 20); /* pointerCacheSize */
	}
}

/* Process pointer capability set */
void
rdp_process_pointer_capset(rdpRdp * rdp, STREAM s)
{
        uint16 colorPointerFlags;
        uint16 colorPointerCacheSize;
        // uint16 pointerCacheSize;
	in_uint16(s, colorPointerFlags); // colorPointerFlags (assumed to be always true)
	in_uint16_le(s, colorPointerCacheSize); // colorPointerCacheSize
        // int_uint16_le(s, pointerCacheSize); // pointerCacheSize
}

/* Output share capability set */
void
rdp_out_share_capset(STREAM s)
{
	rdp_out_capset_header(s, CAPSET_TYPE_SHARE, CAPSET_LEN_SHARE);

	out_uint16(s, 0); // nodeID
	out_uint16(s, 0); // pad
}

/* Process share capability set */
void
rdp_process_share_capset(rdpRdp * rdp, STREAM s)
{
        uint16 nodeID;
	in_uint16(s, nodeID); // nodeID
	// pad
}

/* Output color cache capability set */
void
rdp_out_colorcache_capset(STREAM s)
{
	rdp_out_capset_header(s, CAPSET_TYPE_COLORCACHE, CAPSET_LEN_COLORCACHE);

	out_uint16_le(s, 6);	/* cache size */
	out_uint16(s, 0);	/* pad */
}

/* Process color cache capability set */
void
rdp_process_colorcache_capset(rdpRdp * rdp, STREAM s)
{
        uint16 cacheSize;
	in_uint16_le(s, cacheSize); // cacheSize
        // pad (2 bytes)
}

/* Output brush capability set */
void
rdp_out_brush_capset(STREAM s)
{
	rdp_out_capset_header(s, CAPSET_TYPE_BRUSH, CAPSET_LEN_BRUSH);

	out_uint32_le(s, BRUSH_COLOR_FULL); // brushSupportLevel
}

/* Output glyph cache definition */
void
rdp_out_cache_definition(STREAM s, uint16 cacheEntries, uint16 cacheMaximumCellSize)
{
	out_uint16_le(s, cacheEntries); // cacheEntries
	out_uint16_le(s, cacheMaximumCellSize); // cacheMaximumCellSize
}

/* Output glyph cache capability set */
void
rdp_out_glyphcache_capset(STREAM s)
{
	rdp_out_capset_header(s, CAPSET_TYPE_GLYPHCACHE, CAPSET_LEN_GLYPHCACHE);

	/*
		glyphCache (40 bytes):
		An array of 10 cache definition structures
		Maximum number of cache entries: 254
		Maximum size of a cache element: 2048	
	 */
	rdp_out_cache_definition(s, 0x00FE, 0x0004);
	rdp_out_cache_definition(s, 0x00FE, 0x0004);
	rdp_out_cache_definition(s, 0x00FE, 0x0008);
	rdp_out_cache_definition(s, 0x00FE, 0x0008);
	rdp_out_cache_definition(s, 0x00FE, 0x0010);
	rdp_out_cache_definition(s, 0x00FE, 0x0020);
	rdp_out_cache_definition(s, 0x00FE, 0x0040);
	rdp_out_cache_definition(s, 0x00FE, 0x0080);
	rdp_out_cache_definition(s, 0x00FE, 0x0100);
	rdp_out_cache_definition(s, 0x0040, 0x0800);

	/*
		fragCache (4 bytes):
		Fragment cache data (one cache definition structure)
		Maximum number of cache entries: 256
		Maximum size of a cache element: 256	
	*/
	rdp_out_cache_definition(s, 0x0040, 0x0800); // fragCache

	out_uint16_le(s, GLYPH_SUPPORT_FULL); // glyphSupportLevel
	out_uint16(s, 0); // pad
}

/* Output sound capability set */
void
rdp_out_sound_capset(STREAM s)
{
	rdp_out_capset_header(s, CAPSET_TYPE_SOUND, CAPSET_LEN_SOUND);

	out_uint16_le(s, SOUND_BEEPS_FLAG); // soundFlags
	out_uint16(s, 0); // pad
}

/* Output offscreen cache capability set */
void
rdp_out_offscreenscache_capset(STREAM s)
{
        rdp_out_capset_header(s, CAPSET_TYPE_OFFSCREENCACHE, CAPSET_LEN_OFFSCREENCACHE);

        out_uint32_le(s, 1); // offscreenSupportLevel, either TRUE (0x1) or FALSE (0x0)
        out_uint16_le(s, 7680); // offscreenCacheSize, maximum is 7680 (in KB)
        out_uint16_le(s, 100); // offscreenCacheEntries, maximum is 500 entries
}

/* Output virtual channel capability set */
void
rdp_out_virtualchannel_capset(STREAM s)
{
	rdp_out_capset_header(s, CAPSET_TYPE_VIRTUALCHANNEL, CAPSET_LEN_VIRTUALCHANNEL);

	out_uint32_le(s, VCCAPS_COMPR_SC); // virtual channel compression flags
	out_uint32_le(s, 0); // VCChunkSize, ignored when sent from client to server
}

/* Output virtual channel capability set */
void
rdp_process_virtualchannel_capset(rdpRdp * rdp, STREAM s)
{
	uint32 virtualChannelCompressionFlags;
	uint32 virtualChannelChunkSize;
	in_uint32_le(s, virtualChannelCompressionFlags); // virtual channel compression flags
	in_uint32_le(s, virtualChannelChunkSize); // VCChunkSize
}

/* Output draw ninegrid cache capability set */
void
rdp_out_drawninegridcache_capset(STREAM s)
{
	rdp_out_capset_header(s, CAPSET_TYPE_DRAWNINEGRIDCACHE, CAPSET_LEN_DRAWNINEGRIDCACHE);

        out_uint32_le(s, DRAW_NINEGRID_NO_SUPPORT); // drawNineGridSupportLevel
        out_uint16_le(s, 2560); // drawNineGridCacheSize, maximum is 2560 (in KB)
        out_uint16_le(s, 256); // drawNineGridCacheEntries, maximum is 256
}

/* Output GDI+ cache entries structure */
void
rdp_out_gdiplus_cache_entries(STREAM s)
{
	out_uint16_le(s, 10); // gdipGraphicsCacheEntries
	out_uint16_le(s, 5); // gdipBrushCacheEntries
	out_uint16_le(s, 5); // gdipPenCacheEntries
	out_uint16_le(s, 10); // gdipImageCacheEntries
	out_uint16_le(s, 2); // gdipImageAttributesCacheEntries
}

/* Output GDI+ cache chunk size structure */
void
rdp_out_gdiplus_cache_chunk_size(STREAM s)
{
	out_uint16_le(s, 512); // gdipGraphicsCacheChunkSize
	out_uint16_le(s, 2048); // gdipObjectBrushCacheChunkSize
	out_uint16_le(s, 1024); // gdipObjectPenCacheChunkSize
	out_uint16_le(s, 64); // gdipObjectImageAttributesCacheChunkSize
}

/* Output GDI+ image cache properties structure */
void
rdp_out_gdiplus_image_cache_properties(STREAM s)
{
	out_uint16_le(s, 4096); // gdipObjectImageCacheChunkSize
	out_uint16_le(s, 256); // gdipObjectImageCacheTotalSize
	out_uint16_le(s, 128); // gdipObjectImageCacheMaxSize
}

/* Output draw GDI+ capability set */
void
rdp_out_draw_gdiplus_capset(STREAM s)
{
	rdp_out_capset_header(s, CAPSET_TYPE_DRAWGDIPLUS, CAPSET_LEN_DRAWGDIPLUS);

        out_uint32_le(s, DRAW_GDIPLUS_DEFAULT); // drawGDIPlusSupportLevel
	out_uint32_le(s, 0); // gdipVersion, build number for the GDI+ 1.1 subsystem
	out_uint32_le(s, DRAW_GDIPLUS_CACHE_LEVEL_DEFAULT); // drawGdiplusCacheLevel

	rdp_out_gdiplus_cache_entries(s); // gdipCacheEntries
	rdp_out_gdiplus_cache_chunk_size(s); // gdipCacheChunkSize
	rdp_out_gdiplus_image_cache_properties(s); // gdipImageCacheProperties
}

/* Process GDI+ cache entries structure */
void
rdp_process_gdiplus_cache_entries(rdpRdp * rdp, STREAM s)
{
	uint16 gdipGraphicsCacheEntries;
	uint16 gdipBrushCacheEntries;
	uint16 gdipPenCacheEntries;
	uint16 gdipImageCacheEntries;
	uint16 gdipImageAttributesCacheEntries;

	in_uint16_le(s, gdipGraphicsCacheEntries); // gdipGraphicsCacheEntries
	in_uint16_le(s, gdipBrushCacheEntries); // gdipBrushCacheEntries
	in_uint16_le(s, gdipPenCacheEntries); // gdipPenCacheEntries
	in_uint16_le(s, gdipImageCacheEntries); // gdipImageCacheEntries
	in_uint16_le(s, gdipImageAttributesCacheEntries); // gdipImageAttributesCacheEntries
}

/* Process GDI+ cache chunk size structure */
void
rdp_process_gdiplus_cache_chunk_size(rdpRdp * rdp, STREAM s)
{
	uint16 gdipGraphicsCacheChunkSize;
	uint16 gdipObjectBrushCacheChunkSize;
	uint16 gdipObjectPenCacheChunkSize;
	uint16 gdipObjectImageAttributesCacheChunkSize;

	in_uint16_le(s, gdipGraphicsCacheChunkSize); // gdipGraphicsCacheChunkSize
	in_uint16_le(s, gdipObjectBrushCacheChunkSize); // gdipObjectBrushCacheChunkSize
	in_uint16_le(s, gdipObjectPenCacheChunkSize); // gdipObjectPenCacheChunkSize
	in_uint16_le(s, gdipObjectImageAttributesCacheChunkSize); // gdipObjectImageAttributesCacheChunkSize
}

/* Process GDI+ image cache properties structure */
void
rdp_process_gdiplus_image_cache_properties(rdpRdp * rdp, STREAM s)
{
	uint16 gdipObjectImageCacheChunkSize;
	uint16 gdipObjectImageCacheTotalSize;
	uint16 gdipObjectImageCacheMaxSize;

	in_uint16_le(s, gdipObjectImageCacheChunkSize); // gdipObjectImageCacheChunkSize
	in_uint16_le(s, gdipObjectImageCacheTotalSize); // gdipObjectImageCacheTotalSize
	in_uint16_le(s, gdipObjectImageCacheMaxSize); // gdipObjectImageCacheMaxSize
}


/* Process draw GDI+ capability set */
void
rdp_process_draw_gdiplus_capset(rdpRdp * rdp, STREAM s)
{
	uint32 drawGDIPlusSupportLevel;
	uint32 gdipVersion;
	uint32 drawGdiplusCacheLevel;

        in_uint32_le(s, drawGDIPlusSupportLevel); // drawGDIPlusSupportLevel
	in_uint32_le(s, gdipVersion); // gdipVersion, build number for the GDI+ 1.1 subsystem
	in_uint32_le(s, drawGdiplusCacheLevel); // drawGdiplusCacheLevel

	rdp_process_gdiplus_cache_entries(rdp, s); // gdipCacheEntries
	rdp_process_gdiplus_cache_chunk_size(rdp, s); // gdipCacheChunkSize
	rdp_process_gdiplus_image_cache_properties(rdp, s); // gdipImageCacheProperties
}

/* Output RAIL capability set */
void
rdp_out_rail_capset(STREAM s)
{
	rdp_out_capset_header(s, CAPSET_TYPE_RAIL, CAPSET_LEN_RAIL);

	out_uint32_le(s, RAIL_LEVEL_SUPPORTED); // RailSupportLevel
}

/* Process RAIL capability set */
void
rdp_process_rail_capset(rdpRdp * rdp, STREAM s)
{
	uint32 railSupportLevel;
	in_uint32_le(s, railSupportLevel); // railSupportLevel
}

/* Output window list capability set */
void
rdp_out_window_capset(STREAM s)
{
	rdp_out_capset_header(s, CAPSET_TYPE_WINDOW, CAPSET_LEN_WINDOW);

	out_uint32_le(s, WINDOW_LEVEL_SUPPORTED); // wndSupportLevel
	out_uint8(s, 3); // numIconCaches
	out_uint16_le(s, 12); // numIconCacheEntries
}

/* Process window list capability set */
void
rdp_process_window_capset(rdpRdp * rdp, STREAM s)
{
	uint32 wndSupportLevel;
	uint8 numIconCaches;
	uint16 numIconCacheEntries;

	in_uint32_le(s, wndSupportLevel); // wndSupportLevel
	in_uint8(s, numIconCaches); // numIconCaches
	in_uint16_le(s, numIconCacheEntries); // numIconCacheEntries
}

/* Output large pointer capability set */
void
rdp_out_large_pointer_capset(STREAM s)
{
	rdp_out_capset_header(s, CAPSET_TYPE_LARGE_POINTER, CAPSET_LEN_LARGE_POINTER);

	out_uint16_le(s, LARGE_POINTER_FLAG_96x96); // largePointerSupportFlags
}

/* Output desktop composition capability set */
void
rdp_out_compdesk_capset(STREAM s)
{
	rdp_out_capset_header(s, CAPSET_TYPE_COMPDESK, CAPSET_LEN_COMPDESK);

	out_uint16_le(s, COMPDESK_NOT_SUPPORTED); // CompDeskSupportLevel
}

/* Process desktop composition capability set */
void
rdp_process_compdesk_capset(rdpRdp * rdp, STREAM s)
{
	uint16 compDeskSupportLevel;
	in_uint16_le(s, compDeskSupportLevel); // compDeskSupportLevel
}

/* Output multifragment update capability set */
void
rdp_out_multifragmentupdate_capset(STREAM s)
{
	rdp_out_capset_header(s, CAPSET_TYPE_MULTIFRAGMENTUPDATE, CAPSET_LEN_MULTIFRAGMENTUPDATE);

        /*
         * MaxRequestsize (4 bytes):
         * The size of the buffer used to reassemble the fragments of a
         * fast-path update. The size of this buffer places a cap on the
         * size of the largest fast-path update that can be fragmented.
         */
	out_uint32_le(s, 0x2000); // maxRequestSize
}

/* Process multifragment update capability set */
void
rdp_process_multifragmentupdate_capset(rdpRdp * rdp, STREAM s)
{
	uint32 maxRequestSize;
	in_uint32_le(s, maxRequestSize); // maxRequestSize
}

/* Output surface commands capability set */
void
rdp_out_surface_commands_capset(STREAM s)
{
	rdp_out_capset_header(s, CAPSET_TYPE_SURFACE_COMMANDS, CAPSET_LEN_SURFACE_COMMANDS);

	out_uint32_le(s, SURFCMDS_SETSURFACEBITS); // cmdFlags
        out_uint32(s, 0); // reserved for future use
}

/* Output a bitmap codec structure */
void
rdp_out_bitmap_codec(STREAM s)
{
        // codecGUID (16 bytes)
        out_uint32_le(s, 0); // codecGUID1
        out_uint16_le(s, 0); // codecGUID2
        out_uint16_le(s, 0); // codecGUID3
        out_uint8(s, 0); // codecGUID4
        out_uint8(s, 0); // codecGUID5
        out_uint8(s, 0); // codecGUID6
        out_uint8(s, 0); // codecGUID7
        out_uint8(s, 0); // codecGUID8
        out_uint8(s, 0); // codecGUID9
        out_uint8(s, 0); // codecGUID10
        out_uint8(s, 0); // codecGUID11

        out_uint8(s, 0); // codecID
        out_uint32_le(s, 0); // codecPropertiesLength
        out_uint8s(s, 0); // codecProperties
}

/* Output bitmap codecs capability set */
void
rdp_out_bitmap_codecs_capset(STREAM s)
{
	rdp_out_capset_header(s, CAPSET_TYPE_BITMAP_CODECS, CAPSET_LEN_BITMAP_CODECS);

        out_uint8(s, 0); // bitmapCodecCount, the number of bitmap codec entries

        // bitmapCodecArray
        // rdp_out_bitmap_codec(s, ...);
}


/*
 * Device redirection capability sets
 */


/* Output device redirection capability set header */
void
rdp_out_dr_capset_header(STREAM s, uint16 capabilityType, uint16 capabilityLength, uint32 version)
{
	out_uint16_le(s, capabilityType); // capabilityType
	out_uint16_le(s, capabilityLength); // capabilityLength
	out_uint32_le(s, version); // version
}

/* Output device direction general capability set */
void
rdp_out_dr_general_capset(STREAM s)
{
	rdp_out_dr_capset_header(s,
		DR_CAPSET_TYPE_GENERAL,
		DR_CAPSET_LEN_GENERAL,
		DR_GENERAL_CAPABILITY_VERSION_01);

	out_uint32_le(s, 0); // osType, ignored on receipt
	out_uint32_le(s, 0); // osVersion, unused and must be set to zero
	out_uint16_le(s, 1); // protocolMajorVersion, must be set to 1
	out_uint16_le(s, DR_MINOR_RDP_VERSION_5_2); // protocolMinorVersion
	out_uint32_le(s, 0x0000FFFF); // ioCode1
	out_uint32_le(s, 0); // ioCode2, must be set to zero, reserved for future use
	out_uint32_le(s, DR_DEVICE_REMOVE_PDUS | DR_CLIENT_DISPLAY_NAME_PDU); // extendedPDU
	out_uint32_le(s, DR_ENABLE_ASYNCIO); // extraFlags1
	out_uint32_le(s, 0); // extraFlags2, must be set to zero, reserved for future use

	/*
	 * SpecialTypeDeviceCap (4 bytes):
	 * present when DR_GENERAL_CAPABILITY_VERSION_02 is used
	 */
}

/* Process device direction general capability set */
void
rdp_process_dr_general_capset(STREAM s)
{
	uint16 capabilityLength;
	uint32 version;
	
	uint16 protocolMinorVersion;
	uint32 ioCode1;
	uint32 extendedPDU;
	uint32 extraFlags1;

	in_uint16_le(s, capabilityLength); // capabilityLength
	in_uint32_le(s, version); // version

	in_uint8s(s, 4); // osType, ignored on receipt
	in_uint8s(s, 4); // osVersion, unused and must be set to zero
	in_uint8s(s, 2); // protocolMajorVersion, must be set to 1
	in_uint16_le(s, protocolMinorVersion); // protocolMinorVersion
	in_uint32_le(s, ioCode1); // ioCode1
	in_uint8s(s, 4); // ioCode2, must be set to zero, reserved for future use
	in_uint32_le(s, extendedPDU); // extendedPDU
	in_uint32_le(s, extraFlags1); // extraFlags1
	in_uint8s(s, 4); // extraFlags2, must be set to zero, reserved for future use

	/*
	 * SpecialTypeDeviceCap (4 bytes):
	 * present when DR_GENERAL_CAPABILITY_VERSION_02 is used
	 */

	if(version == DR_GENERAL_CAPABILITY_VERSION_02)
	{
		uint32 specialTypeDeviceCap;
		in_uint32_le(s, specialTypeDeviceCap);
	}
}

/* Output printer direction capability set */
void
rdp_out_dr_printer_capset(STREAM s)
{
	rdp_out_dr_capset_header(s,
		DR_CAPSET_TYPE_PRINTER,
		DR_CAPSET_LEN_PRINTER,
		DR_GENERAL_CAPABILITY_VERSION_01);
}

/* Process printer direction capability set */
void
rdp_process_dr_printer_capset(STREAM s)
{
	uint16 capabilityLength;
	uint32 version;

	in_uint16_le(s, capabilityLength); // capabilityLength
	in_uint32_le(s, version); // version
}

/* Output port redirection capability set */
void
rdp_out_dr_port_capset(STREAM s)
{
	rdp_out_dr_capset_header(s,
		DR_CAPSET_TYPE_PORT,
		DR_CAPSET_LEN_PORT,
		DR_GENERAL_CAPABILITY_VERSION_01);
}

/* Process port redirection capability set */
void
rdp_process_dr_port_capset(STREAM s)
{
	uint16 capabilityLength;
	uint32 version;

	in_uint16_le(s, capabilityLength); // capabilityLength
	in_uint32_le(s, version); // version
}

/* Output drive redirection capability set */
void
rdp_out_dr_drive_capset(STREAM s)
{
	rdp_out_dr_capset_header(s,
		DR_CAPSET_TYPE_DRIVE,
		DR_CAPSET_LEN_DRIVE,
		DR_GENERAL_CAPABILITY_VERSION_01);

	/*
	 * [MS-RDPEFS] says DR_GENERAL_CAPABILITY_VERSION_02 must be used
	 * with DR_CAPSET_TYPE_DRIVE, but changing it to the correct version
	 * breaks drive redirection
	 */
}

/* Process drive redirection capability set */
void
rdp_process_dr_drive_capset(STREAM s)
{
	uint16 capabilityLength;
	uint32 version;

	in_uint16_le(s, capabilityLength); // capabilityLength
	in_uint32_le(s, version); // version
}

/* Output smart card redirection capability set */
void
rdp_out_dr_smartcard_capset(STREAM s)
{
	rdp_out_dr_capset_header(s,
		DR_CAPSET_TYPE_SMARTCARD,
		DR_CAPSET_LEN_SMARTCARD,
		DR_GENERAL_CAPABILITY_VERSION_01);
}

/* Process smartcard redirection capability set */
void
rdp_process_dr_smartcard_capset(STREAM s)
{
	uint16 capabilityLength;
	uint32 version;

	in_uint16_le(s, capabilityLength); // capabilityLength
	in_uint32_le(s, version); // version
}


