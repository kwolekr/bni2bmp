/*-
 * Copyright (c) 2009 Ryan Kwolek
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright notice, this list of
 *     conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice, this list
 *     of conditions and the following disclaimer in the documentation and/or other materials
 *     provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//bni2bmp <source file> [output file]

////////////////////////[Preprocessor Directives]////////////////////////////

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>

#define PKT_RUNLEN   0x80
#define TGA_SCRORGN  0x20
#define PKT_SIZEBITS 0x7F

#define SWAP(a,b) ((a) ^= (b) ^= (a) ^= (b))


//////////////////////////[Struct Declarations]//////////////////////////////

typedef struct _bniheader {
	int headersize;
	short version;
	short reserved;
	int numicons;
	int dataoffset;
} BNIHEADER, *LPBNIHEADER;

typedef struct _iconheader {
	int flags;
	int cx;
	int cy;
} ICONHEADER, *LPICONHEADER;

typedef struct _tgaheader {
	unsigned char infolen;
	unsigned char colormaptype;
	unsigned char imagetype;
	unsigned char colormapdata[5];
	unsigned short xorigin;
	unsigned short yorigin;
	unsigned short cx;
	unsigned short cy;
	unsigned char depth;
	unsigned char descriptor;  
} TGAHEADER, *LPTGAHEADER;

typedef struct _clrpacket {
	char pheader;
	char b;
	char g;
	char r;
} CLRPACKET, *LPCLRPACKET;

typedef struct _rgbpacket {
	char b;
	char g;
	char r;
} RGBPACKET, *LPRGBPACKET;


////////////////////////////////[Globals]////////////////////////////////////

char srcfile[MAX_PATH];
char destfile[MAX_PATH];


///////////////////////////////[Functions]///////////////////////////////////

int *TGADecompress(FILE *file, int size) {
	CLRPACKET clr;
	RGBPACKET rgb;
	int *buf = (int *)malloc(size * sizeof(int)), curr = 0, i;

	while (curr < size) {
		fread(&clr, sizeof(CLRPACKET), 1, file);
		if (clr.pheader & PKT_RUNLEN) {		   
			for (i = 0; i < ((clr.pheader & PKT_SIZEBITS) + 1); i++) {
				buf[curr++] = RGB(clr.r, clr.g, clr.b);
			}																					 
		} else {
			buf[curr++] = RGB(clr.r, clr.g, clr.b);
			for (i = 0; i < (clr.pheader & PKT_SIZEBITS); i++) {
				fread(&rgb, sizeof(rgb), 1, file);
				buf[curr++] = RGB(rgb.r, rgb.g, rgb.b);
			}
		}
	}
	return buf;
}			


HBITMAP ReadBNIToBitmap(char *filename) {
	int numpixels = 0;
	BNIHEADER bni;
	TGAHEADER tga;
	ICONHEADER ico;
	BITMAPINFO bmi;

	FILE *file = fopen(filename, "rb");
	if (!file)
		return NULL;

	fread(&bni, sizeof(BNIHEADER), 1, file);
	char asdf[512];
	printf("header size: %d\nversion: %x\nnumicons: %d\ndataoffset: %x\n", 
		bni.headersize, bni.version, bni.numicons, bni.dataoffset);
	
	for (int i = 0; i != bni.numicons; i++) {
		int pcount = 0;
		char tmpclient[8];
		tmpclient[4] = 0;
		fread(&ico, sizeof(ICONHEADER), 1, file);
		if (!ico.flags) {
			int *products = (int *)malloc(sizeof(int));
			fread(products, sizeof(int), 1, file);
			while (products[pcount]) {
				*(int *)tmpclient = products[pcount];
				strrev(tmpclient);
				printf("client: %s\n", tmpclient);
				pcount++;
				products = (int *)realloc(products, (pcount + 1) * sizeof(int));
				fread(&products[pcount], sizeof(int), 1, file);
			} 
			free(products);				
		} else { 
			printf("flags: 0x%02x\n", ico.flags);
			fseek(file, sizeof(int), SEEK_CUR);
		}
		printf("size: %dx%d\n", ico.cx, ico.cy);	
	}
	fseek(file, bni.dataoffset, SEEK_SET);
	fread(&tga, sizeof(TGAHEADER), 1, file);

	printf("size: %dx%d\n"
			"depth: %d\n"
			"origin: %d, %d\n"
			"infolen: %d\n"
			"datatype: %d\n"
			"colormap type: %d\n"
			"screen origin: %ser left corner\n", 
			tga.cx, tga.cy, tga.depth, tga.xorigin, tga.yorigin,
			tga.infolen, tga.imagetype, tga.colormaptype,
			(tga.descriptor & TGA_SCRORGN) ? "upp" : "low");

	if (tga.depth != 24)
		printf("WARNING! Only supports 24 bit tgas supported!\n");
	if (tga.imagetype != 10)
		printf("WARNING! Only run length encoded true color rgb tgas supported!\n");
	if (tga.infolen) {
		char *tgainfo = (char *)malloc(tga.infolen + 1);
		fread(tgainfo, 1, tga.infolen, file);
		tgainfo[tga.infolen] = 0;
		printf(asdf, "tga info: %s\n", tgainfo);
		free(tgainfo);
	}	
	int *tehpic = TGADecompress(file, tga.cy * tga.cx);
	fclose(file);
	
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = tga.cx;
	bmi.bmiHeader.biHeight = tga.cy;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biSizeImage = 0;
	bmi.bmiHeader.biXPelsPerMeter = 0;
	bmi.bmiHeader.biYPelsPerMeter = 0;
	bmi.bmiHeader.biClrUsed = 0;
	bmi.bmiHeader.biClrImportant = 0;
	bmi.bmiColors[0].rgbBlue = 0;
	bmi.bmiColors[0].rgbGreen = 0;
	bmi.bmiColors[0].rgbRed = 0;
	bmi.bmiColors[0].rgbReserved = 0;

	int pxnum = 0;
	for (int y = 0; y != tga.cy; y++) {
		for (int x = 0; x != tga.cx; x++) {
			char *blah = (char *)&tehpic[pxnum];
			SWAP(blah[0], blah[2]);
			pxnum++;
		}
	}

	HDC hdc = GetDC(NULL);			  
	HBITMAP hBmp = CreateCompatibleBitmap(hdc, tga.cx, tga.cy);
	SetDIBits(hdc, hBmp, 0, tga.cy, tehpic, &bmi, DIB_RGB_COLORS);
	ReleaseDC(NULL, hdc);
	free(tehpic);
	return hBmp;
}		


void WriteBitmapToFile(HBITMAP hBmp, char *filename) {
	BITMAPFILEHEADER bfh;
	BITMAPINFO bi;
	BITMAP bmp;

	GetObject(hBmp, sizeof(BITMAP), &bmp);

	int imgsize = bmp.bmWidth * bmp.bmHeight * sizeof(RGBQUAD);
			   
	bfh.bfType = 'MB';
	bfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + imgsize;
	bfh.bfReserved1 = 0;
	bfh.bfReserved2 = 0;
	bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	
	bi.bmiHeader.biSize		     = sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biWidth	     = bmp.bmWidth;
	bi.bmiHeader.biHeight	     = bmp.bmHeight;
	bi.bmiHeader.biPlanes		 = bmp.bmPlanes;
	bi.bmiHeader.biBitCount  	 = bmp.bmBitsPixel;
	bi.bmiHeader.biCompression	 = BI_RGB;
	bi.bmiHeader.biSizeImage 	 = imgsize;
	bi.bmiHeader.biXPelsPerMeter = 0;
	bi.bmiHeader.biYPelsPerMeter = 0;
	bi.bmiHeader.biClrUsed		 = 0;
	bi.bmiHeader.biClrImportant  = 0;
	*(int *)(&bi.bmiColors[0])   = 0;
	
	int *lpImgData = (int *)malloc(imgsize);
	HDC hdc = GetDC(NULL);
	GetDIBits(hdc, hBmp, 0, bmp.bmHeight, lpImgData, &bi, DIB_RGB_COLORS);
	ReleaseDC(NULL, hdc);

	FILE *file = fopen(filename, "wb");
	if (!file)
		return;
	fwrite(&bfh, sizeof(BITMAPFILEHEADER), 1, file);
	fwrite(&bi.bmiHeader, sizeof(BITMAPINFOHEADER), 1, file);
	fwrite(lpImgData, bi.bmiHeader.biSizeImage, 1, file);
	fclose(file);

	free(lpImgData);
}


int main(int argc, char *argv[]) {
	if (argc < 2) {
		printf("Invalid number of parameters!\n");
		return 0;
	} else if (argc == 2) {
		GetEnvironmentVariable("userprofile", destfile, sizeof(destfile));	  
		strncat(destfile, "\\Desktop\\", sizeof(destfile));
		strncpy(srcfile, argv[1], sizeof(srcfile));
		char *tmp = strrchr(srcfile, '\\');
		if (!tmp)
			return 0;
		strncat(destfile, tmp, sizeof(destfile));
		if (strlen(tmp) <= 4)
			return 0;
		*(int *)(destfile + strlen(destfile) - 4) = 'pmb.';
	} else {
		strncpy(srcfile, argv[1], sizeof(srcfile));
		strncpy(destfile, argv[2], sizeof(destfile));
	}

	WriteBitmapToFile(ReadBNIToBitmap(srcfile), destfile);
	return 0;
}

