#include <windows.h>
#include <conio.h>
#include <stdio.h>

#include "funciones.h"
#include "zip.h"
#include "unzip.h"
#include <png.h>

using namespace std;

char string[64] = { 0 };
int TablesOffsets[20] = {0};

HZIP hz; DWORD writ;

typedef byte*  cache;
static cache writeData;
static unsigned int current = 0;

//**************************************************************
//**************************************************************
//  Png_WriteData
//
//  Work with data writing through memory
//**************************************************************
//**************************************************************

static void Png_WriteData(png_structp png_ptr, cache data, size_t length) {
    writeData = (byte*)realloc(writeData, current + length);
    memcpy(writeData + current, data, length);
    current += length;
}

//**************************************************************
//**************************************************************
//  Png_Create
//
//  Create a PNG image through memory
//**************************************************************
//**************************************************************

cache Png_Create(cache data, int* size, int width, int height, int paloffset, int offsetx = 0, int offsety = 0, bool sky = false)
{
    int i, j;
    cache image;
    cache out;
    cache* row_pointers;
    png_structp png_ptr;
    png_infop info_ptr;
    png_colorp palette;
    
    int bit_depth = 8;
    // setup png pointer
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    if(png_ptr == NULL) {
        error("Png_Create: Failed getting png_ptr");
        return NULL;
    }

    // setup info pointer
    info_ptr = png_create_info_struct(png_ptr);
    if(info_ptr == NULL) {
        png_destroy_write_struct(&png_ptr, NULL);
        error("Png_Create: Failed getting info_ptr");
        return NULL;
    }

    // what does this do again?
    if(setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        error("Png_Create: Failed on setjmp");
        return NULL;
    }

    // setup custom data writing procedure
    png_set_write_fn(png_ptr, NULL, Png_WriteData, NULL);

    // setup image
    png_set_IHDR(
        png_ptr,
        info_ptr,
        width,
        height,
        bit_depth,
        PNG_COLOR_TYPE_PALETTE,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT);

    // setup palette
    FILE *fpal = fopen ("Datos/newPalettes.bin","rb");
    if(!fpal)
    {
       error("No puede abrir newPalettes.bin");
    }

    if(sky)
    {
       fclose(fpal);
       
       fpal = fopen ("Datos/tables.bin","rb");
       if(!fpal)
          error("No puede abrir tables.bin");
          
       fseek(fpal, TablesOffsets[paloffset],SEEK_SET);
       int size = ReadUint(fpal);
    }
    else
        fseek(fpal,(paloffset),SEEK_SET);
        
    palette = (png_colorp) malloc((256*3)*png_sizeof(png_color));
     
    int gettrans = -1;
    for(int x = 0;x < 256;x++)
    {
       int Palbit = ReadWord(fpal);
       int B = (B = Palbit & 0x1F) << 3 | B >> 2;
       int G = (G = Palbit >> 5 & 0x3F) << 2 | G >> 4;
       int R = (R = Palbit >> 11 & 0x1F) << 3 | R >> 2;
 
       palette[x].red = R;
       palette[x].green = G;
       palette[x].blue = B;
       
       if(gettrans == -1)
       {
          if(palette[x].red==255 && palette[x].green==0 && palette[x].blue==255)
             gettrans = x;
          else if(palette[x].red==255 && palette[x].green==4 && palette[x].blue==255)
             gettrans = x;
       }
    }
    fclose(fpal);
    
    png_set_PLTE(png_ptr, info_ptr,palette,256);
    
    if(gettrans != -1)
    {
        png_byte trans[gettrans+1]; 
        for(int tr =0;tr < gettrans+1; tr++)
        {
          if(tr==gettrans){trans[tr]=0;}
          else {trans[tr]=255;}
        }
        png_set_tRNS(png_ptr, info_ptr,trans,gettrans+1,NULL);
    }

    // add png info to data
    png_write_info(png_ptr, info_ptr);

    if(offsetx !=0 || offsety !=0)
    {
       int offs[2];
    
       offs[0] = Swap32(offsetx);
       offs[1] = Swap32(offsety);
    
       png_write_chunk(png_ptr, (png_byte*)"grAb", (byte*)offs, 8);
    }

    // setup packing if needed
    png_set_packing(png_ptr);
    png_set_packswap(png_ptr);

    // copy data over
    byte inputdata;
    image = data;
    row_pointers = (cache*)malloc(sizeof(byte*) * height);
    for(i = 0; i < height; i++)
    {
        row_pointers[i] = (cache)malloc(width);
        for(j = 0; j < width; j++)
        {
            inputdata = *image;
            //if(inputdata == 0x7f){inputdata = (byte)gettrans;}
            row_pointers[i][j] = inputdata;
            image++;
        }
    }

    // cleanup
    png_write_image(png_ptr, row_pointers);
    png_write_end(png_ptr, info_ptr);
    free((void**)&palette);
    free((void**)row_pointers);
    palette = NULL;
    row_pointers = NULL;
    png_destroy_write_struct(&png_ptr, &info_ptr);

    // allocate output
    out = (cache)malloc(current);
    memcpy(out, writeData, current);
    *size = current;

    free(writeData);
    writeData = NULL;
    current = 0;

    return out;
}

//--------Flip--------//
static void flip(byte *input, byte *output, int width, int height)
{
    int i;
    int length = (width*height);
    //printf("sizeof(input) %d\n",length);
    
    byte pixel;
    int count = 0;
    int offset = 1;
    for(int i=0; i < height; i++)
    {
        for(int j=0; j < width; j++)
        {
            pixel = (byte)input[count];
            output[(length-width*offset)+j] = (byte)pixel;
            count++;
        }
        offset+=1;
    }
}

//--------Rotar 90º Y Flip--------//
static void rotate90andflip(byte *input, byte *output, int width, int height)
{
    int i;
    int length = (width*height);
    int height0 = height;//width
    int width0 = length / height;//height
    //printf("sizeof(input) %d\n",length);
    
    int bit1 = width0;
    int bit2 = 1;
    int bit3 = 0;
    
    int offset = length;
    
    byte pixel;
    for(i = 0; i < length; i++)
    {
    pixel = (byte)input[i];
    output[length - offset + bit3] = (byte)pixel;
    
    if(bit2 == height0){offset = length;}
    else{offset = length - bit1;}
    
    if(bit2 == height0){bit2 = 0; bit3 += 1;}
    bit2 +=1;
    bit1 = width0 * bit2;
    }
}

static int pngcount2 = 0;
void Create_Walls(int file, int startoff,int count,int paloffset, int x0, int x1, int y0, int y1)
{
    //printf("x0 %d x1 %d y0 %d y1 %d\n",x0,x1,y0,y1);
    
    int width = x1-x0;
    int width2 = 256;
    if(width <= 256){width2 = 256;}
    if(width <= 128){width2 = 128;}
    if(width <= 64){width2 = 64;}
    if(width <= 32){width2 = 32;}
    if(width <= 16){width2 = 16;}
    if(width <= 8){width2 = 8;}
    if(width <= 4){width2 = 4;}
    if(width <= 0){width2 = 256;}
    width += (width2-width);
    int height = count/width;

    //printf("width %d height %d\n",width,height);
    
    sprintf(string,"Datos/newTexels%3.3d.bin",file);
    
    FILE *in;
    in = fopen(string,"rb");
    if(!in)
    {
     error("No puede abrir %s", string);
    }
    //FILE *out = fopen("tex.raw","wb");

    cache input;
    cache output;
    cache pngout;
    
    input = (byte*)malloc(count);
    output = (byte*)malloc(count);
    
    for(int a = 0; a < (width*height); a++)
    {
       input[a] = 0x00;
    }

    fseek(in, startoff,SEEK_SET);
    byte pixel = 0;
    for(int a = 0; a < count; a++)
    {
       pixel = ReadByte(in);
       input[a] = pixel;
    }
    
    flip(input, output, width, height);

    int pngsize = 0;
    pngout = Png_Create(output, &pngsize,width,height, paloffset);

    sprintf(string,"WTEX/WTEX%03d",pngcount2);
    ZipAdd(hz, string, pngout, pngsize);
    pngcount2++;
    
    fclose(in);
    free(input);
    free(output);
    free(pngout);
    //getch();
}

static int pngcount = 0;
void Create_Sprites(int file, int startoff,int count,int paloffset, int x0, int x1, int y0, int y1)
{
    //printf("x0 %d x1 %d y0 %d y1 %d\n",x0,x1,y0,y1);
    
    int width = x1-x0;
    int height = y1-y0;
    
    //printf("width %d height %d\n",width,height);
    
    sprintf(string,"Datos/newTexels%3.3d.bin",file);
    
    FILE *in;
    in = fopen(string,"rb");
    if(!in)
    {
     error("No puede abrir %s", string);
    }
    
    int endoffset = startoff+count-6;
    fseek(in, endoffset,SEEK_SET);
    
    int shapeoffset = endoffset - ReadWord(in);
    fseek(in, shapeoffset,SEEK_SET);
    
    //printf("endoffset %d\n",endoffset);
    //printf("shapeoffset %d\n",shapeoffset);
    
    int i = 0;
    int j = 0;
    int k = 0;
    int linecount = 0;
    int shapetype[256] = {0};
    byte shape;
    byte shapeBit;
    for(i = 0; i < width; i++)
    {
       if(!(i & 1))
          shape = ReadByte(in);
          
          
       if(!(i & 1))
          shapeBit = (byte)(shape & 0xF);
       else 
          shapeBit = (byte)(shape >> 4 & 0xF);
     
       shapetype[i] = shapeBit;
    }
    
    linecount = i;
    //printf("linecount %d\n",linecount);
    
    cache input;
    cache output;
    cache pngout;
    
    //height = 176;//con y offset puesto
    input = (byte*)malloc(width*height);
    output = (byte*)malloc(width*height);
    
    for(int a = 0; a < (width*height); a++)
    {
       input[a] = 0x00;
    }
    
    int pos;
    int cnt;
    for(i = 0; i < linecount; i++)
    {
       if(shapetype[i] != 0)
       {
           for(j = 0; j < shapetype[i]; j++)
           {
               //pos = ReadByte(in);//con y offset puesto
               pos = (ReadByte(in) - y0)+1;
               cnt = ReadByte(in);
               for(k = 0; k < cnt; k++)
               {
           	      input[(i*height)+ pos+k] = 0xff;
               }
           }
       }
       /*else
       {
           pos = 0;//con y offset puesto
           cnt = 176;
           for(k = 0; k < cnt; k++)
           {
       	      input[(i*height)+ pos+k] = 0x00;
           }
       }*/
    }
    
    fseek(in, startoff,SEEK_SET);
    byte pixel = 0;
    for(int a = 0; a < (width*height); a++)
    {
       if(input[a] == 0xff)
       {
          pixel = ReadByte(in);
          input[a] = pixel;
       }
    }
    
    rotate90andflip(input, output, width, height);

    int offsetx = (88-x0);
    int offsety = (176-y0)+1;
    int pngsize = 0;
    pngout = Png_Create(output, &pngsize,width,height, paloffset, offsetx, offsety);

    sprintf(string,"STEX/STEX%03d",pngcount);
    ZipAdd(hz, string, pngout, pngsize);
    pngcount++;
    
    fclose(in);
    free(input);
    free(output);
    free(pngout);
}

static sword Unk01[512];
static byte Unk02[1024];
static word Sizes[4096];//x1x2y1y2
static uint Pals[1024];
static uint Tex[1024];

static void initflags(int paramInt)
{
    int i1 = Unk01[paramInt];
    paramInt = Unk01[(paramInt + 1)];
    //printf("i1 %d paramInt %d\n",i1,paramInt);
    while (i1 < paramInt)
    {
      int i2 = i1;
      if ((Pals[i2] & 0x80000000) != 0) {
        i2 = Pals[i2] & 0x3FF;
      }
      int tmp53_51 = i2;
      //printf("Paleta %d\n",tmp53_51);//pal
      uint tmp53_50 = Pals[tmp53_51];
      Pals[tmp53_51] = ((uint)(tmp53_50 | 0x40000000));
      
      i2 = i1;
      if ((Tex[i2] & 0x80000000) != 0) {
        i2 = Tex[i2] & 0x3FFF;
      }
      int tmp87_85 = i2;
      //printf("Texture %d\n",tmp87_85);//tex
      uint tmp87_84 = Tex[tmp87_85];
      Tex[tmp87_85] = ((uint)(tmp87_84 | 0x40000000));
      i1++;
    }
}

static int SpriteOffset[4096][3] = {0,0,0};
static int TextureOffset[4096][3] = {0,0,0};
static int PalsOffset[4096] = {0};


static void inittex()
{
    int i5 = 0;
    int i6 = 1;
    int i4 = 0;
    int i8 = 0;
    int i9 = 0;
    int i10 = 0;
    int i11 = 0;
    int i12 = 0;
    int i7 = 0;
    for (i7 = 0; i7 < 1024; i7++)
    {
      i8 = (Tex[i7] & 0x40000000) != 0 ? 1 : 0;
      i9 = (Pals[i7] & 0x40000000) != 0 ? 1 : 0;
      
      //printf("i7 %d\n",i7);
      //printf("i8 %d\n",i8);//tex
      //printf("i9 %d\n",i9);//Pals
      if (i8 != 0)
      {
        if ((((i8 = i10 = (Tex[i7] & 0xFFFF) + 1) - 1 & i8) == 0 ? 1 : 0) != 0)
        {
          //printf("i6 %d i10 %d\n",i6,i10+4);//Tex
          int tmp141_139 = i7;
          Tex[tmp141_139] = ((uint)(Tex[tmp141_139] | 0x20000000));
        }
        else
        {
          i4 += i10;
        }
      }
      if (i9 != 0)
      {
        if ((i10 = Pals[i7] & 0x3FFF) < 256) {
          i10 = 256;
        }
        //printf("i5 %d i10 %d\n",i6,i10);//Pals
        i5++;
      }
      //getch();
    }
    
    //printf("\n\n\n");
    i5 = 0;
    uint paloffs = 0;
    for (i7 = 0; i7 < 1024; i7++)
    {
      i8 = (Pals[i7] & 0x40000000) != 0 ? 1 : 0;
      i9 = (Pals[i7] & 0x80000000) != 0 ? 1 : 0;
      i10 = (Pals[i7] & 0xFFFF)+2;
      
      //printf("i8 %d i9 %d i10 %d\n",i8,i9,i10);//Pals
      if ((i8 != 0) && (i9 == 0))
      {
        //printf("paloffs %d i5 %d\n",paloffs,i5);//Pals
        PalsOffset[i5] = paloffs;
        i11 = (uint)(i7 | 0x80000000);
        for (i12 = i7 + 1; i12 < 1024; i12++) {
          if (Pals[i12] == i11) {
            Pals[i12] = ((uint)(0xC0000000 | i5));
          }
        }
        Pals[i7] = ((uint)(0x40000000 | i5));
        i5++;
        paloffs += (i10 << 1);
      }
      else if (i9 == 0)
      {
        //printf("i9 Pals i10 %d\n",i10);//Pals
      }
      //getch();
    }
    i6 = 1;
    i7 = -1;
    i8 = 0;
    i9 = 0;
    i10 = 0;
    i11 = 0;
    i12 = 0;
    
    int i15 = 0;
    //printf("\n\n\n");
    for (int i1 = 0; i1 < 1024; i1++)
    {
      int i2 = (Tex[i1] & 0x40000000) != 0 ? 1 : 0;
      i5 = (Tex[i1] & 0x80000000) != 0 ? 1 : 0;
      int i13 = (Tex[i1] & 0xFFFF) + 1 + 4;
      int i14 = (Tex[i1] & 0x20000000) != 0 ? 1 : 0;
      
      //printf("i2 %d i5 %d i13 %d i14 %d\n",i2,i5,i13,i14);//tex
      
      if ((i2 != 0) && (i5 == 0))
      {
        if (i10 != i7)
        {
          i7 = i10;
          i8 = 0;
          //printf("i7 %d\n",i7);//tex
          i11 = 0;//gec
          i15 = 0;//gec
        }
        if (i8 != i9) {
          //printf("i9 - i8 %d\n",i9 - i8);//tex
        }
        int i3 = (uint)(i1 | 0x80000000);
        if (i14 != 0)
        {
          TextureOffset[i6][0] = i7;
          TextureOffset[i6][1] = i15;
          TextureOffset[i6][2] = i13;
          //printf("i1 %d i6 %d i13 %d\n",i1,i6,i13);//tex
          i5 = (uint)(0x40000000 | 0x20000000 | i6);
        }
        else
        {
          SpriteOffset[i12][0] = i7;
          SpriteOffset[i12][1] = i11;
          SpriteOffset[i12][2] = i13;
          //printf("i1 %d i12 %d i11 %d i13 %d\n",i1,i12,i11,i13);//tex
          i5 = (uint)(0x40000000 | i12);
        }
        for (i8 = i1 + 1; i8 < 1024; i8++) {
          if (Tex[i8] == i3) {
            Tex[i8] = ((uint)(i5 | 0x80000000));
          }
        }
        Tex[i1] = i5;
        if (i14 != 0)
        {
          i15 += i13;
          i11 += i13;//
          i6++;
        }
        else
        {
          i15 += i13;//
          i11 += i13;
          i12++;
        }
        i8 = i9 += i13;
      }
      else if (i5 == 0)
      {
        i9 += i13;
      }
      if (i9 > 262144)
      {
        i10++;
        i9 = 0;
      }
      //getch();
    }
}

static void extraer(int paramInt1)
{
    int i1 = paramInt1;
    int file = 0;
    int startoff = 0;
    int count = 0;
    bool sprite = false;
    if ((Tex[i1] & 0x20000000) == 0)
    {
        paramInt1 = Tex[i1] & 0x1FFF;
        file = SpriteOffset[paramInt1][0];
        startoff = SpriteOffset[paramInt1][1];
        count = SpriteOffset[paramInt1][2];
        sprite = true;
        //printf("paramInt1 %d\n",paramInt1);
        //printf("File %d\n",SpriteOffset[paramInt1][0]);
        //printf("OffsetS %d\n",SpriteOffset[paramInt1][1]);
        //printf("OffsetE %d\n",SpriteOffset[paramInt1][2]);
    }
    else
    {
        int paramInt = Tex[i1] & 0x1FFF;
        file = TextureOffset[paramInt][0];
        startoff = TextureOffset[paramInt][1];
        count = TextureOffset[paramInt][2];
        sprite = false;
        //printf("File %d\n",TextureOffset[paramInt][0]);
        //printf("OffsetS %d\n",TextureOffset[paramInt][1]);
        //printf("OffsetE %d\n",TextureOffset[paramInt][2]);
    }
    int paramInt = Pals[i1] & 0x3FF;
    //printf("pal %d\n",paramInt);//arrayOfInt1[(Pals[i1] & 0x3FF)]);
    //printf("PalsOffset %d\n",PalsOffset[paramInt]);
    //paramInt2 = (paramInt1 = Unk02[i1]) >> 4 & 0xF;
    //paramInt1 &= 0xF;
    int x0 = (Sizes[(i1 << 2)] & 0xFF);
    int x1  = (Sizes[((i1 << 2) + 1)] & 0xFF);
    int y0  = (Sizes[((i1 << 2) + 2)] & 0xFF);
    int y1  = (Sizes[((i1 << 2) + 3)] & 0xFF);
    //printf("x0 %d x1 %d y0 %d y1 %d\n",x0,x1,y0,y1);
  
    if (sprite)
    {
        Create_Sprites(file,startoff,count,PalsOffset[paramInt], x0, x1, y0, y1);
    }
    else
    {
        Create_Walls(file,startoff,count,PalsOffset[paramInt], x0, x1, y0, y1);
    }
}

void ReadTablesOffset()
{
    FILE *f1 = fopen("Datos/tables.bin","rb");
    if(!f1)
    {
     error("No puede abrir tables.bin");
    }
    
    int LastOffset = 0;
    int NextOffset = 0;
    for(int i = 0; i <20; i++)
    {
       NextOffset = ReadUint(f1)+ 80;
       TablesOffsets[i] = LastOffset;
       LastOffset = NextOffset;
    }
    fclose(f1);
}

void ExtraerSKYS(int idx, int num)
{
    FILE *f1 = fopen("Datos/tables.bin","rb");
    if(!f1)
    {
     error("No puede abrir tables.bin");
    }
    
    PrintfPorcentaje(1,1,true, 12+num,"Extrayendo SKY%02d......",num);
    
    fseek(f1, TablesOffsets[idx],SEEK_SET);
    int size = ReadUint(f1);
    
    cache input = (byte*)malloc(size);
    for(int a = 0; a < size; a++)
       input[a] = ReadByte(f1);
       
    int pngsize = 0;
    cache pngout = Png_Create(input, &pngsize, 256, 256, idx-1, 0, 0, true);

    sprintf(string,"SKYS/SKY%02d",num);
    ZipAdd(hz, string, pngout, pngsize);
    pngcount++;
    
    fclose(f1);
    free(input);
    free(pngout);
}

void ShowInfo()
{
    setcolor(0x07);printf("     ############");
    setcolor(0x0A);printf("(ERICK194)");
    setcolor(0x07);printf("#############\n"); 
    printf("     # DOOM II RPG (IPHONE) EXTRACTOR  #\n");
    printf("     # CREADO POR ERICK VASQUEZ GARCIA #\n");
    printf("     #   ES PARA DOOM II RPG (IPHONE)  #\n");
    printf("     #          VERSION 1.0            #\n");
    printf("     #          MODO DE USO:           #\n");
    printf("     #  SOLO NECESITAS EL ARCHIVO IPA  #\n");
    printf("     ###################################\n");
    printf("\n");
}

void ExtraerDatos()
{
    hz = OpenZip(("DOOM_II_RPG_1.0.ipa"),0);
    if(hz == NULL){error("No encuentra DOOM_II_RPG_1.0.ipa");}
    SetUnzipBaseDir(hz,("Datos"));
    ZIPENTRY ze; GetZipItem(hz,-1,&ze); int numitems=ze.index;
    
    int i;
    FindZipItem(hz,("Payload/Doom2rpg.app/Packages/newMappings.bin"),true,&i,&ze);
    UnzipItem(hz,i,ze.name+30);
    FindZipItem(hz,("Payload/Doom2rpg.app/Packages/newPalettes.bin"),true,&i,&ze);
    UnzipItem(hz,i,ze.name+30);
    FindZipItem(hz,("Payload/Doom2rpg.app/Packages/tables.bin"),true,&i,&ze);
    UnzipItem(hz,i,ze.name+30);
    
    for(int j = 0; j <= 38; j++)
    {
        PrintfPorcentaje(j,38-1,true, 10,"Extrayendo Datos......");
        sprintf(string,"Payload/Doom2rpg.app/Packages/newTexels%3.3d.bin",j);
        FindZipItem(hz,string,true,&i,&ze);
        UnzipItem(hz,i,ze.name+30);
    }
    
    CloseZip(hz);
}

int main(int argc, char *argv[])
{
    ShowInfo();

    FILE *f1;
    
    f1 = fopen("Datos/newMappings.bin","rb");
    if(!f1)
    {
     ExtraerDatos();
    }
    fclose(f1);
    
    
    f1 = fopen("Datos/newMappings.bin","rb");
    if(!f1)
    {
     error("No puede abrir newMappings.bin");
    }
    
    hz = CreateZip("DoomIIRpgIphone.pk3",0);
    
    int i = 0;
    
    for(i = 0; i < 512; i++)
    {
        Unk01[i] = ReadSword(f1);
    }
    for(i = 0; i < 1024; i++)
    {
        Unk02[i] = ReadByte(f1);
    }
    for(i = 0; i < 4096; i++)
    {
        Sizes[i] = ReadWord(f1);
    }
    for(i = 0; i < 1024; i++)
    {
        Pals[i] = ReadUint(f1);
    }
    for(i = 0; i < 1024; i++)
    {
        Tex[i] = ReadUint(f1);
    }
    fclose(f1);
    
    for(i = 0; i < 512; i++)
    {
      if(Unk01[i] != -1)
      {
       if(Unk01[i+1] == -1) continue;
       initflags(i);
       //getch();
      }
    }
    
    inittex();
    
    for(i = 0; i < 512; i++)
    {
      PrintfPorcentaje(i,512-1,true, 11,"Extrayendo Sprites Y Texturas......");
      //printf("i %d\n",i);
      if(Unk01[i] != -1)
      {
       if(Unk01[i+1] == -1) continue;
       //printf("\ni %4.4X\n",i);
       
       int i1 = Unk01[i];
       int paramInt = Unk01[(i + 1)];
       
       while (i1 < paramInt)
       {
         extraer(i1);
         //getch();
         i1++;
       }
      }
    }
    fclose(f1);
    
    ReadTablesOffset();
    ExtraerSKYS(17, 0);
    ExtraerSKYS(19, 1);

    CloseZip(hz);
    system("PAUSE");
    return EXIT_SUCCESS;
}
