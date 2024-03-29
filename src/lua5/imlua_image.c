/** \file
 * \brief IM Lua 5 Binding
 *
 * See Copyright Notice in im_lib.h
 */

#include <string.h>
#include <memory.h>
#include <stdlib.h>

#include "im.h"
#include "im_image.h"
#include "im_util.h"
#include "im_convert.h"
#include "im_palette.h"

#include <lua.h>
#include <lauxlib.h>

#include "imlua.h"
#include "imlua_image.h"
#include "imlua_palette.h"
#include "imlua_aux.h"


static imImage** imlua_rawcheckimage(lua_State *L, int param)
{
  return (imImage**) luaL_checkudata(L, param, "imImage");
}

imImage* imlua_checkimage(lua_State *L, int param)
{
  imImage** image_p = imlua_rawcheckimage(L, param);

  if (!(*image_p))
    luaL_argerror(L, param, "destroyed imImage");

  return *image_p;
}

int imlua_pushimageerror(lua_State *L, imImage* image, int error)
{
  if (error)
  {
    lua_pushnil(L);  /* one for the image */
    imlua_pusherror(L, error);  /* one for the error */
    return 2;
  }
  else
  {
    imlua_pushimage(L, image);
    return 1;   /* the error will be nil */
  }
}

void imlua_pushimage(lua_State *L, imImage* image)
{
  if (!image)
  {
    luaL_error(L, "image failed to be created, insufficient memory");
  }
  else
  {
    imImage **image_p = (imImage**) lua_newuserdata(L, sizeof(imImage*));
    *image_p = image;
    luaL_getmetatable(L, "imImage");
    lua_setmetatable(L, -2);
  }
}

/*****************************************************************************\
 image channel, for indexing
\*****************************************************************************/
static imluaImageChannel *imlua_newimagechannel (lua_State *L, imImage *image, int channel)
{
  imluaImageChannel* image_channel = (imluaImageChannel*) lua_newuserdata(L, sizeof(imluaImageChannel));
  image_channel->image = image;
  image_channel->channel = channel;
  luaL_getmetatable(L, "imImageChannel");
  lua_setmetatable(L, -2);
  return image_channel;
}

static imluaImageChannel* imlua_checkimagechannel (lua_State *L, int param)
{
  return (imluaImageChannel*) luaL_checkudata(L, param, "imImageChannel");
}

/*****************************************************************************\
 image lin, for indexing
\*****************************************************************************/
static imluaImageLine* imlua_newimageline (lua_State *L, imImage *image, int channel, int lin)
{
  imluaImageLine* image_line = (imluaImageLine*) lua_newuserdata(L, sizeof(imluaImageLine));
  image_line->image = image;
  image_line->channel = channel;
  image_line->lin = lin;
  luaL_getmetatable(L, "imImageChannelLine");
  lua_setmetatable(L, -2);
  return image_line;
}

static imluaImageLine* imlua_checkimageline (lua_State *L, int param)
{
  return (imluaImageLine*) luaL_checkudata(L, param, "imImageChannelLine");
}

/*****************************************************************************\
 im.ImageCreate(width, height, color_space, data_type)
\*****************************************************************************/
static int imluaImageCreate (lua_State *L)
{
  int width = luaL_checkinteger(L, 1);
  int height = luaL_checkinteger(L, 2);
  int color_space = luaL_checkinteger(L, 3);
  int data_type = luaL_checkinteger(L, 4);
  imImage *image;

  if (!imImageCheckFormat(color_space, data_type))
    luaL_error(L, "invalid combination of color space and data type.");

  image = imImageCreate(width, height, color_space, data_type);
  imlua_pushimage(L, image);
  return 1;
}

static int imluaImageSetPixels(lua_State *L)
{
  int i, n, total_count;
  imImage* image = imlua_checkimage(L, 1);
  int depth = image->depth;
  if (image->has_alpha) depth++;
  if (image->data_type == IM_CFLOAT || image->data_type == IM_CDOUBLE) depth *= 2;

  luaL_checktype(L, 2, LUA_TTABLE);

  n = imlua_getn(L, 2);
  total_count = image->width * image->height * depth;
  if (total_count != n)
    luaL_error(L, "number of elements in the table must be width*height*depth of the image.");

  for (i = 0; i < n; i++)
  {
    lua_rawgeti(L, 2, i+1);

    if (image->data_type == IM_FLOAT || image->data_type == IM_CFLOAT)
    {
      lua_Number value = luaL_checknumber(L, -1);
      float* fdata = (float*)image->data[0];
      fdata[i] = (float)value;
    }
    else if (image->data_type == IM_DOUBLE || image->data_type == IM_CDOUBLE)
    {
      lua_Number value = luaL_checknumber(L, -1);
      double* fdata = (double*)image->data[0];
      fdata[i] = (double)value;
    }
    else
    {
      int value = luaL_checkinteger(L, -1);

      switch (image->data_type)
      {
      case IM_BYTE:
        {
          imbyte *bdata = (imbyte*)image->data[0];
          bdata[i] = (imbyte)value;
          break;
        }
      case IM_SHORT:
        {
          short *sdata = (short*)image->data[0];
          sdata[i] = (short)value;
          break;
        }
      case IM_USHORT:
        {
          imushort *udata = (imushort*)image->data[0];
          udata[i] = (imushort)value;
          break;
        }
      case IM_INT:
        {
          int *idata = (int*)image->data[0];
          idata[i] = (int)value;
          break;
        }
      }
    }

    lua_pop(L, 1);
  }

  return 0;
}

static int imluaImageGetPixels(lua_State *L)
{
  int i, total_count;
  imImage* image = imlua_checkimage(L, 1);
  int depth = image->depth;
  if (image->has_alpha) depth++;
  if (image->data_type == IM_CFLOAT || image->data_type == IM_CDOUBLE) depth *= 2;

  total_count = image->width * image->height * depth;
  lua_createtable(L, total_count, 0);

  for (i = 0; i < total_count; i++)
  {
    if (image->data_type == IM_FLOAT || image->data_type == IM_CFLOAT)
    {
      float* fdata = (float*)image->data[0];
      lua_pushnumber(L, (lua_Number)fdata[i]);
    }
    else if (image->data_type == IM_DOUBLE || image->data_type == IM_CDOUBLE)
    {
      double* fdata = (double*)image->data[0];
      lua_pushnumber(L, (lua_Number)fdata[i]);
    }
    else
    {
      lua_Integer value = 0;

      switch (image->data_type)
      {
      case IM_BYTE:
        {
          imbyte *bdata = (imbyte*)image->data[0];
          value = (lua_Integer)bdata[i];
          break;
        }
      case IM_SHORT:
        {
          short *sdata = (short*)image->data[0];
          value = (lua_Integer)sdata[i];
          break;
        }
      case IM_USHORT:
        {
          imushort *udata = (imushort*)image->data[0];
          value = (lua_Integer)udata[i];
          break;
        }
      case IM_INT:
        {
          int *idata = (int*)image->data[0];
          value = (lua_Integer)idata[i];
          break;
        }
      }

      lua_pushinteger(L, value);
    }

    lua_rawseti(L, -2, i + 1);
  }

  return 1;
}

/*****************************************************************************\
 im.ImageCreateFromOpenGLData(width, height, glformat, gldata)
\*****************************************************************************/
static int imluaImageCreateFromOpenGLData (lua_State *L)
{
  int width = luaL_checkinteger(L, 1);
  int height = luaL_checkinteger(L, 2);
  int glformat = luaL_checkinteger(L, 3);
  void* gldata = lua_touserdata(L, 4);
  imImage *image = imImageCreateFromOpenGLData(width, height, glformat, gldata);
  imlua_pushimage(L, image);
  return 1;
}

/*****************************************************************************\
 image:AddAlpha()
\*****************************************************************************/
static int imluaImageAddAlpha (lua_State *L)
{
  imImageAddAlpha(imlua_checkimage(L, 1));
  return 0;
}

static int imluaImageRemoveAlpha (lua_State *L)
{
  imImageRemoveAlpha(imlua_checkimage(L, 1));
  return 0;
}

/*****************************************************************************\
 image:SetAlpha(alpha: number)
\*****************************************************************************/
static int imluaImageSetAlpha (lua_State *L)
{
  imImageSetAlpha(imlua_checkimage(L, 1), (float)luaL_checknumber(L, 2));
  return 0;
}

/*****************************************************************************\
 image:Reshape()
\*****************************************************************************/
static int imluaImageReshape (lua_State *L)
{
  imImage* image = imlua_checkimage(L, 1);
  int width = luaL_checkinteger(L, 2);
  int height = luaL_checkinteger(L, 3);

  imImageReshape(image, width, height);
  return 0;
}

/*****************************************************************************\
 image:Copy()
\*****************************************************************************/
static int imluaImageCopy (lua_State *L)
{
  imImage* src_image = imlua_checkimage(L, 1);
  imImage* dst_image = imlua_checkimage(L, 2);

  imlua_match(L, src_image, dst_image);
  imImageCopy(src_image, dst_image);
  return 0;
}

/*****************************************************************************\
 image:CopyData()
\*****************************************************************************/
static int imluaImageCopyData (lua_State *L)
{
  imImage* src_image = imlua_checkimage(L, 1);
  imImage* dst_image = imlua_checkimage(L, 2);

  imlua_match(L, src_image, dst_image);
  imImageCopyData(src_image, dst_image);
  return 0;
}

/*****************************************************************************\
 image:CopyPlane()
\*****************************************************************************/
static int imluaImageCopyPlane(lua_State *L)
{
  imImage* src_image = imlua_checkimage(L, 1);
  int src_plane = luaL_checkinteger(L, 2);
  imImage* dst_image = imlua_checkimage(L, 3);
  int dst_plane = luaL_checkinteger(L, 4);
  int src_depth, dst_depth;

  imlua_matchdatatype(L, src_image, dst_image);

  src_depth = src_image->has_alpha? src_image->depth+1: src_image->depth;
  if (src_plane < 0 || src_plane >= src_depth)
    luaL_argerror(L, 2, "invalid source channel, out of bounds");

  dst_depth = dst_image->has_alpha? dst_image->depth+1: dst_image->depth;
  if (dst_plane < 0 || dst_plane >= dst_depth)
    luaL_argerror(L, 4, "invalid target channel, out of bounds");

  imImageCopyPlane(src_image, src_plane, dst_image, dst_plane);
  return 0;
}

/*****************************************************************************\
 image:Duplicate()
\*****************************************************************************/
static int imluaImageDuplicate (lua_State *L)
{
  imImage* image = imlua_checkimage(L, 1);
  imImage *new_image = imImageDuplicate(image);
  imlua_pushimage(L, new_image);
  return 1;
}

/*****************************************************************************\
 image:Clone()
\*****************************************************************************/
static int imluaImageClone (lua_State *L)
{
  imImage* image = imlua_checkimage(L, 1);
  imImage *new_image = imImageClone(image);
  imlua_pushimage(L, new_image);
  return 1;
}

/*****************************************************************************\
 image:SetAttribute(attrib, data_type, data)
\*****************************************************************************/
static int imluaImageSetAttribute (lua_State *L)
{
  int i, count = 0;
  void *data = NULL;

  imImage* image = imlua_checkimage(L, 1);
  const char *attrib = luaL_checkstring(L, 2);
  int data_type = luaL_checkinteger(L, 3);

  if (!lua_isnil(L, 4))
  {
    if (!lua_isstring(L, 4))
    {
      luaL_checktype(L, 4, LUA_TTABLE);
      count = imlua_getn(L, 4);
      data = malloc(imDataTypeSize(data_type) * count); 
    } 
    else if (data_type != IM_BYTE) 
      luaL_argerror(L, 4, "if value is string, then data type must be byte"); 

    switch (data_type)
    {
    case IM_BYTE:
      {
        if (lua_isstring(L, 4))
        {
          const char* str = lua_tostring(L, 4);
          count = (int)strlen(str)+1;
          data = malloc(imDataTypeSize(data_type) * count);
          memcpy(data, str, count);
        }
        else
        {
          imbyte *data_byte = (imbyte*) data;
          for (i = 0; i < count; i++)
          {
            lua_rawgeti(L, 4, i+1);
            data_byte[i] = (imbyte)luaL_checkinteger(L, -1);
            lua_pop(L, 1);
          }
        }
      }
      break;

    case IM_SHORT:
      {
        short *data_short = (short*) data;
        for (i = 0; i < count; i++)
        {
          lua_rawgeti(L, 4, i+1);
          data_short[i] = (short)luaL_checkinteger(L, -1);
          lua_pop(L, 1);
        }
      }
      break;

    case IM_USHORT:
      {
        imushort *data_ushort = (imushort*) data;
        for (i = 0; i < count; i++)
        {
          lua_rawgeti(L, 4, i+1);
          data_ushort[i] = (imushort)luaL_checkinteger(L, -1);
          lua_pop(L, 1);
        }
      }
      break;

    case IM_INT:
      {
        int *data_int = (int*) data;
        for (i = 0; i < count; i++)
        {
          lua_rawgeti(L, 4, i+1);
          data_int[i] = luaL_checkinteger(L, -1);
          lua_pop(L, 1);
        }
      }
      break;

    case IM_FLOAT:
      {
        float *data_float = (float*) data;
        for (i = 0; i < count; i++)
        {
          lua_rawgeti(L, 4, i+1);
          data_float[i] = (float) luaL_checknumber(L, -1);
          lua_pop(L, 1);
        }
      }
      break;

    case IM_CFLOAT:
      {
        float *data_float = (float*) data;
        for (i = 0; i < count; i++)
        {
          int two;
          float *value = imlua_toarrayfloat(L, -1, &two, 1);
          if (two != 2)
          {
            free(value);
            luaL_argerror(L, 4, "invalid value");
          }

          data_float[i] = value[0];
          data_float[i+1] = value[1];
          free(value);
          lua_pop(L, 1);
        }        
      }
      break;

    case IM_DOUBLE:
      {
        double *data_double = (double*) data;
        for (i = 0; i < count; i++)
        {
          lua_rawgeti(L, 4, i+1);
          data_double[i] = (double) luaL_checknumber(L, -1);
          lua_pop(L, 1);
        }
      }
      break;

    case IM_CDOUBLE:
      {
        double *data_double = (double*) data;
        for (i = 0; i < count; i++)
        {
          int two;
          double *value = imlua_toarraydouble(L, -1, &two, 1);
          if (two != 2)
          {
            free(value);
            luaL_argerror(L, 4, "invalid value");
          }

          data_double[i] = value[0];
          data_double[i+1] = value[1];
          free(value);
          lua_pop(L, 1);
        }        
      }
      break;
    }
  }

  imImageSetAttribute(image, attrib, data_type, count, data);
  return 0;
}

static int imluaImageSetAttribInteger(lua_State *L)
{
  imImage *iimage = imlua_checkimage(L, 1);
  const char *attrib = luaL_checkstring(L, 2);
  int data_type = luaL_checkinteger(L, 3);
  int value = luaL_checkinteger(L, 4);
  imImageSetAttribInteger(iimage, attrib, data_type, value);
  return 0;
}

static int imluaImageSetAttribReal(lua_State *L)
{
  imImage *iimage = imlua_checkimage(L, 1);
  const char *attrib = luaL_checkstring(L, 2);
  int data_type = luaL_checkinteger(L, 3);
  double value = luaL_checknumber(L, 4);
  imImageSetAttribReal(iimage, attrib, data_type, value);
  return 0;
}

static int imluaImageSetAttribString(lua_State *L)
{
  imImage *iimage = imlua_checkimage(L, 1);
  const char *attrib = luaL_checkstring(L, 2);
  const char* value = luaL_checkstring(L, 3);
  imImageSetAttribString(iimage, attrib, value);
  return 0;
}


/*****************************************************************************\
 image:GetAttribute(attrib)
\*****************************************************************************/
static int imluaImageGetAttribute (lua_State *L)
{
  int data_type;
  int i, count;
  const void *data;
  int as_string = 0;

  imImage* image = imlua_checkimage(L, 1);
  const char *attrib = luaL_checkstring(L, 2);

  data = imImageGetAttribute(image, attrib, &data_type, &count);
  if (!data)
  {
    lua_pushnil(L);
    return 1;
  }

  if (data_type == IM_BYTE && lua_isboolean(L, 3))
    as_string = lua_toboolean(L, 3);

  if (!as_string)
    lua_newtable(L);
  
  switch (data_type)
  {
  case IM_BYTE:
    {
      if (as_string)
      {
        lua_pushstring(L, (const char*)data);
      }
      else
      {
        imbyte *data_byte = (imbyte*) data;
        for (i = 0; i < count; i++, data_byte++)
        {
          lua_pushnumber(L, *data_byte);
          lua_rawseti(L, -2, i+1);
        }
      }
    }
    break;

  case IM_SHORT:
    {
      short *data_short = (short*) data;
      for (i = 0; i < count; i++, data_short += 2)
      {
        lua_pushnumber(L, *data_short);
        lua_rawseti(L, -2, i+1);
      }
    }
    break;

  case IM_USHORT:
    {
      imushort *data_ushort = (imushort*) data;
      for (i = 0; i < count; i++, data_ushort += 2)
      {
        lua_pushnumber(L, *data_ushort);
        lua_rawseti(L, -2, i+1);
      }
    }
    break;

  case IM_INT:
    {
      int *data_int = (int*) data;
      for (i = 0; i < count; i++, data_int++)
      {
        lua_pushnumber(L, *data_int);
        lua_rawseti(L, -2, i+1);
      }
    }
    break;

  case IM_FLOAT:
    {
      float *data_float = (float*) data;
      for (i = 0; i < count; i++, data_float++)
      {
        lua_pushnumber(L, *data_float);
        lua_rawseti(L, -2, i+1);
      }
    }
    break;

  case IM_CFLOAT:
    {
      float *data_float = (float*) data;
      for (i = 0; i < count; i++, data_float += 2)
      {
        imlua_newarrayfloat(L, data_float, 2, 1);
        lua_rawseti(L, -2, i+1);
      }        
    }
    break;

  case IM_DOUBLE:
    {
      double *data_double = (double*) data;
      for (i = 0; i < count; i++, data_double++)
      {
        lua_pushnumber(L, *data_double);
        lua_rawseti(L, -2, i+1);
      }
    }
    break;

  case IM_CDOUBLE:
    {
      double *data_double = (double*) data;
      for (i = 0; i < count; i++, data_double += 2)
      {
        imlua_newarraydouble(L, data_double, 2, 1);
        lua_rawseti(L, -2, i+1);
      }        
    }
    break;
  }

  lua_pushnumber(L, data_type);

  return 2;
}

static int imluaImageGetAttribInteger(lua_State *L)
{
  imImage *iimage = imlua_checkimage(L, 1);
  const char *attrib = luaL_checkstring(L, 2);
  int index = luaL_optinteger(L, 3, 0);
  int value = imImageGetAttribInteger(iimage, attrib, index);
  lua_pushinteger(L, value);
  return 1;
}

static int imluaImageGetAttribReal(lua_State *L)
{
  imImage *iimage = imlua_checkimage(L, 1);
  const char *attrib = luaL_checkstring(L, 2);
  int index = luaL_optinteger(L, 3, 0);
  double value = imImageGetAttribReal(iimage, attrib, index);
  lua_pushnumber(L, value);
  return 1;
}

static int imluaImageGetAttribString(lua_State *L)
{
  imImage *iimage = imlua_checkimage(L, 1);
  const char *attrib = luaL_checkstring(L, 2);
  const char *value = imImageGetAttribString(iimage, attrib);
  lua_pushstring(L, value);
  return 1;
}

/*****************************************************************************\
 image:GetAttributeList()
\*****************************************************************************/
static int imluaImageGetAttributeList (lua_State *L)
{
  int i, attrib_count;
  char **attrib;

  imImage* image = imlua_checkimage(L, 1);

  imImageGetAttributeList(image, NULL, &attrib_count);

  attrib = (char**) malloc(attrib_count * sizeof(char*));

  imImageGetAttributeList(image, attrib, &attrib_count);

  lua_createtable(L, attrib_count, 0);
  for (i = 0; i < attrib_count; i++)
  {
    lua_pushstring(L, attrib[i]);
    lua_rawseti(L, -2, i+1);
  }

  return 1;
}

/*****************************************************************************\
 image:Clear()
\*****************************************************************************/
static int imluaImageClear (lua_State *L)
{
  imImageClear(imlua_checkimage(L, 1));
  return 0;
}

/*****************************************************************************\
 image:isBitmap()
\*****************************************************************************/
static int imluaImageIsBitmap (lua_State *L)
{
  lua_pushboolean(L, imImageIsBitmap(imlua_checkimage(L, 1)));
  return 1;
}


/*****************************************************************************\
 image:GetOpenGLData()
\*****************************************************************************/
static int imluaImageGetOpenGLData (lua_State *L)
{
  int format;
  imbyte* gldata;
  imImage *image = imlua_checkimage(L, 1);

  gldata = imImageGetOpenGLData(image, &format);
  if (!gldata)
  {
    lua_pushnil(L);
    return 1;
  }

  lua_pushlightuserdata(L, gldata);
  lua_pushinteger(L, format);
  return 2;
}

/*****************************************************************************\
 image:GetPalette()
\*****************************************************************************/
static int imluaImageGetPalette (lua_State *L)
{
  imImage *image = imlua_checkimage(L, 1);
  long* palette = imPaletteNew(256);
  memcpy(palette, image->palette, sizeof(long) * 256);
  imlua_pushpalette(L, palette, 256);
  return 1;
}

/*****************************************************************************\
 image:SetPalette
\*****************************************************************************/
static int imluaImageSetPalette (lua_State *L)
{
  imImage *image = imlua_checkimage(L, 1);
  imluaPalette *pal = imlua_checkpalette(L, 2);
  long* palette = imPaletteNew(256);
  memcpy(palette, pal->color, pal->count*sizeof(long));
  imImageSetPalette(image, palette, pal->count);
  return 0;
}

/*****************************************************************************\
 image:CopyAttributes(dst_image)
\*****************************************************************************/
static int imluaImageCopyAttributes (lua_State *L)
{
  imImage *src_image = imlua_checkimage(L, 1);
  imImage *dst_image = imlua_checkimage(L, 2);

  imImageCopyAttributes(src_image, dst_image);
  return 0;
}

/*****************************************************************************\
 image:MergeAttributes(dst_image)
\*****************************************************************************/
static int imluaImageMergeAttributes (lua_State *L)
{
  imImage *src_image = imlua_checkimage(L, 1);
  imImage *dst_image = imlua_checkimage(L, 2);

  imImageMergeAttributes(src_image, dst_image);
  return 0;
}

/*****************************************************************************\
 image:MatchSize(image2)
\*****************************************************************************/
static int imluaImageMatchSize (lua_State *L)
{
  imImage *image1 = imlua_checkimage(L, 1);
  imImage *image2 = imlua_checkimage(L, 2);

  lua_pushboolean(L, imImageMatchSize(image1, image2));
  return 1;
}

/*****************************************************************************\
 image:MatchColor(image2)
\*****************************************************************************/
static int imluaImageMatchColor (lua_State *L)
{
  imImage *image1 = imlua_checkimage(L, 1);
  imImage *image2 = imlua_checkimage(L, 2);

  lua_pushboolean(L, imImageMatchColor(image1, image2));
  return 1;
}

/*****************************************************************************\
 image:MatchDataType(image2)
\*****************************************************************************/
static int imluaImageMatchDataType (lua_State *L)
{
  imImage *image1 = imlua_checkimage(L, 1);
  imImage *image2 = imlua_checkimage(L, 2);

  lua_pushboolean(L, imImageMatchDataType(image1, image2));
  return 1;
}

/*****************************************************************************\
 image:MatchColorSpace(image2)
\*****************************************************************************/
static int imluaImageMatchColorSpace (lua_State *L)
{
  imImage *image1 = imlua_checkimage(L, 1);
  imImage *image2 = imlua_checkimage(L, 2);

  lua_pushboolean(L, imImageMatchColorSpace(image1, image2));
  return 1;
}

/*****************************************************************************\
 image:Match(image2)
\*****************************************************************************/
static int imluaImageMatch (lua_State *L)
{
  imImage *image1 = imlua_checkimage(L, 1);
  imImage *image2 = imlua_checkimage(L, 2);

  lua_pushboolean(L, imImageMatch(image1, image2));
  return 1;
}

/*****************************************************************************\
 image:SetBinary()
\*****************************************************************************/
static int imluaImageSetBinary (lua_State *L)
{
  imImage *image = imlua_checkimage(L, 1);
  if (image->color_space != IM_MAP && 
      image->color_space != IM_GRAY)
      luaL_argerror(L, 1, "color space must be Map or Gray");
  imlua_checkdatatype(L, 1, image, IM_BYTE);
  imImageSetBinary(image);
  return 0;
}

static int imluaImageSetMap(lua_State *L)
{
  imImage *image = imlua_checkimage(L, 1);
  if (image->color_space != IM_GRAY &&
      image->color_space != IM_BINARY)
      luaL_argerror(L, 1, "color space must be Binary or Gray");
  imlua_checkdatatype(L, 1, image, IM_BYTE);
  imImageSetMap(image);
  return 0;
}

static int imluaImageSetGray(lua_State *L)
{
  imImage *image = imlua_checkimage(L, 1);
  if (image->color_space != IM_MAP && 
      image->color_space != IM_BINARY)
      luaL_argerror(L, 1, "color space must be Map or Binary");
  imlua_checkdatatype(L, 1, image, IM_BYTE);
  imImageSetGray(image);
  return 0;
}

static int imluaImageMakeBinary (lua_State *L)
{
  imImage *image = imlua_checkimage(L, 1);
  imlua_checkdatatype(L, 1, image, IM_BYTE);
  imImageMakeBinary(image);
  return 0;
}

static int imluaImageMakeGray (lua_State *L)
{
  imImage *image = imlua_checkimage(L, 1);
  imlua_checkdatatype(L, 1, image, IM_BYTE);
  imImageMakeGray(image);
  return 0;
}

/*****************************************************************************\
 image:Width()
\*****************************************************************************/
static int imluaImageWidth(lua_State *L)
{
  imImage* image = imlua_checkimage(L, 1);
  lua_pushnumber(L, image->width);
  return 1;
}

/*****************************************************************************\
 image:Height()
\*****************************************************************************/
static int imluaImageHeight(lua_State *L)
{
  imImage* image = imlua_checkimage(L, 1);
  lua_pushnumber(L, image->height);
  return 1;
}

/*****************************************************************************\
 image:Depth()
\*****************************************************************************/
static int imluaImageDepth(lua_State *L)
{
  imImage* image = imlua_checkimage(L, 1);
  lua_pushnumber(L, image->depth);
  return 1;
}

/*****************************************************************************\
 image:DataType()
\*****************************************************************************/
static int imluaImageDataType(lua_State *L)
{
  imImage* image = imlua_checkimage(L, 1);
  lua_pushnumber(L, image->data_type);
  return 1;
}

/*****************************************************************************\
 image:ColorSpace()
\*****************************************************************************/
static int imluaImageColorSpace(lua_State *L)
{
  imImage* image = imlua_checkimage(L, 1);
  lua_pushnumber(L, image->color_space);
  return 1;
}

/*****************************************************************************\
 image:HasAlpha()
\*****************************************************************************/
static int imluaImageHasAlpha(lua_State *L)
{
  imImage* image = imlua_checkimage(L, 1);
  lua_pushboolean(L, image->has_alpha);
  return 1;
}

/*****************************************************************************\
 im.FileImageLoad(filename, [index])
\*****************************************************************************/
static int imluaFileImageLoad (lua_State *L)
{
  const char *filename = luaL_checkstring(L, 1);
  int index = luaL_optinteger(L, 2, 0);
  int error;
  imImage *image = imFileImageLoad(filename, index, &error);
  return imlua_pushimageerror(L, image, error);
}

/*****************************************************************************\
 im.FileImageLoadRegion(filename, [index])
\*****************************************************************************/
static int imluaFileImageLoadRegion (lua_State *L)
{
  const char *filename = luaL_checkstring(L, 1);
  int index = luaL_checkinteger(L, 2);
  int bitmap = luaL_checkinteger(L, 3);
  int xmin = luaL_checkinteger(L, 4);
  int xmax = luaL_checkinteger(L, 5);
  int ymin = luaL_checkinteger(L, 6);
  int ymax = luaL_checkinteger(L, 7);
  int width = luaL_checkinteger(L, 8);
  int height = luaL_checkinteger(L, 9);
  int error;
  imImage *image = imFileImageLoadRegion(filename, index, bitmap, &error, xmin, xmax, ymin, ymax, width, height);
  return imlua_pushimageerror(L, image, error);
}

/*****************************************************************************\
 im.FileImageLoadBitmap(filename, [index])
\*****************************************************************************/
static int imluaFileImageLoadBitmap (lua_State *L)
{
  const char *filename = luaL_checkstring(L, 1);
  int index = luaL_optinteger(L, 2, 0);
  int error;
  imImage *image = imFileImageLoadBitmap(filename, index, &error);
  return imlua_pushimageerror(L, image, error);
}

/*****************************************************************************\
 im.FileImageSave(filename, format, image)
\*****************************************************************************/
static int imluaFileImageSave (lua_State *L)
{
  const char *file_name = luaL_checkstring(L, 1);
  const char *format = imlua_checkformat(L, 2);
  imImage *image = imlua_checkimage(L, 3);

  imlua_pusherror(L, imFileImageSave(file_name, format, image));
  return 1;
}

/*****************************************************************************\
 image:Save(filename, format)
\*****************************************************************************/
static int imluaImageSave (lua_State *L)
{
  imImage *image = imlua_checkimage(L, 1);
  const char *file_name = luaL_checkstring(L, 2);
  const char *format = imlua_checkformat(L, 3);

  imlua_pusherror(L, imFileImageSave(file_name, format, image));
  return 1;
}

/*****************************************************************************\
 image:Destroy()
\*****************************************************************************/
static int imluaImageDestroy (lua_State *L)
{
  imImage** image_p = imlua_rawcheckimage(L, 1);
  if (!(*image_p))
    luaL_argerror(L, 1, "destroyed imImage");

  imImageDestroy(*image_p);
  *image_p = NULL; /* mark as destroyed */
  return 0;
}

/*****************************************************************************\
 gc
\*****************************************************************************/
static int imluaImage_gc (lua_State *L)
{
  imImage** image_p = imlua_rawcheckimage(L, 1);
  if (*image_p)
  {
    imImageDestroy(*image_p);
    *image_p = NULL; /* mark as destroyed */
  }

  return 0;
}

/*****************************************************************************\
 image tostring
\*****************************************************************************/
static int imluaImage_tostring (lua_State *L)
{
  imImage** image_p = (imImage**)lua_touserdata(L, 1);
  if (*image_p)
  {
    imImage *image = *image_p;
    lua_pushfstring(L, "imImage(%p) [width=%d,height=%d,color_space=%s,data_type=%s,depth=%d,has_alpha=%s]", 
      image_p,
      image->width, 
      image->height,
      imColorModeSpaceName(image->color_space),
      imDataTypeName(image->data_type),
      image->depth,
      image->has_alpha? "yes": "no"
    );
  }
  else
  {
    lua_pushfstring(L, "imImage(%p)-destroyed", image_p);
  }

  return 1;
}

/*****************************************************************************\
 image_channel tostring
\*****************************************************************************/
static int imluaImageChannel_tostring (lua_State *L)
{
  imluaImageChannel *image_channel = imlua_checkimagechannel(L, 1);
  lua_pushfstring(L, "imImageChannel(%p) [channel=%d]", 
    image_channel, 
    image_channel->channel
  );
  return 1;
}

/*****************************************************************************\
 image_line tostring
\*****************************************************************************/
static int imluaImageLine_tostring (lua_State *L)
{
  char buff[32];
  imluaImageLine *image_line = imlua_checkimageline(L, 1);

  sprintf(buff, "%p", lua_touserdata(L, 1));
  lua_pushfstring(L, "imImageLine(%s) [channel=%d,lin=%d]", 
    buff, 
    image_line->channel,
    image_line->lin
  );
  return 1;
}

/*****************************************************************************\
 image lin indexing
\*****************************************************************************/
static int imluaImageLine_index (lua_State *L)
{
  int index;
  imluaImageLine *image_line = imlua_checkimageline(L, 1);
  imImage *image = image_line->image;
  int channel = image_line->channel;
  int lin = image_line->lin;
  int column = luaL_checkinteger(L, 2);
  void* channel_buffer = image->data[channel];

  if (column < 0 || column >= image->width)
    luaL_argerror(L, 2, "invalid column, out of bounds");

  index = lin * image->width + column;

  switch (image->data_type)
  {
  case IM_BYTE:
    {
      imbyte *bdata = (imbyte*) channel_buffer;
      lua_pushnumber(L, (lua_Number) bdata[index]);
    }
    break;

  case IM_SHORT:
    {
      short *sdata = (short*) channel_buffer;
      lua_pushnumber(L, (lua_Number) sdata[index]);
    }
    break;

  case IM_USHORT:
    {
      imushort *udata = (imushort*) channel_buffer;
      lua_pushnumber(L, (lua_Number) udata[index]);
    }
    break;

  case IM_INT:
    {
      int *idata = (int*) channel_buffer;
      lua_pushnumber(L, (lua_Number) idata[index]);
    }
    break;

  case IM_FLOAT:
    {
      float *fdata = (float*) channel_buffer;
      lua_pushnumber(L, (lua_Number) fdata[index]);
    }
    break;
    
  case IM_CFLOAT:
    {
      float *cdata = (float*) channel_buffer;
      imlua_newarrayfloat(L, cdata + (2*index), 2, 1);
    }
    break;

  case IM_DOUBLE:
    {
      double *fdata = (double*) channel_buffer;
      lua_pushnumber(L, (lua_Number) fdata[index]);
    }
    break;
    
  case IM_CDOUBLE:
    {
      double *cdata = (double*) channel_buffer;
      imlua_newarraydouble(L, cdata + (2*index), 2, 1);
    }
    break;
  }

  return 1;
}

/*****************************************************************************\
 image lin new index
\*****************************************************************************/
static int imluaImageLine_newindex (lua_State *L)
{
  int index;
  imluaImageLine *image_line = imlua_checkimageline(L, 1);
  imImage *image = image_line->image;
  int channel = image_line->channel;
  int lin = image_line->lin;
  int column = luaL_checkinteger(L, 2);
  void* channel_buffer = image->data[channel];

  if (column < 0 || column >= image->width)
    luaL_argerror(L, 2, "invalid column, out of bounds");

  index = lin * image->width + column;

  switch (image->data_type)
  {
  case IM_BYTE:
    {
      int value = luaL_checkinteger(L, 3);
      imbyte *bdata = (imbyte*) channel_buffer;
      bdata[index] = (imbyte) value;
    }
    break;

  case IM_SHORT:
    {
      int value = luaL_checkinteger(L, 3);
      short *sdata = (short*) channel_buffer;
      sdata[index] = (short) value;
    }
    break;

  case IM_USHORT:
    {
      int value = luaL_checkinteger(L, 3);
      imushort *udata = (imushort*) channel_buffer;
      udata[index] = (imushort) value;
    }
    break;

  case IM_INT:
    {
      int value = luaL_checkinteger(L, 3);
      int *idata = (int*) channel_buffer;
      idata[index] = (int) value;
    }
    break;

  case IM_FLOAT:
    {
      lua_Number value = luaL_checknumber(L, 3);
      float *fdata = (float*) channel_buffer;
      fdata[index] = (float) value;
    }
    break;
    
  case IM_CFLOAT:
    {
      int count;
      float *cdata = (float*) channel_buffer;
      float *value = imlua_toarrayfloat(L, 3, &count, 1);
      if (count != 2)
      {
        free(value);
        luaL_argerror(L, 3, "invalid value");
      }

      cdata[2*index] = value[0];
      cdata[2*index+1] = value[1];
      free(value);
    }
    break;

  case IM_DOUBLE:
    {
      lua_Number value = luaL_checknumber(L, 3);
      double *fdata = (double*) channel_buffer;
      fdata[index] = (double) value;
    }
    break;
    
  case IM_CDOUBLE:
    {
      int count;
      double *cdata = (double*) channel_buffer;
      double *value = imlua_toarraydouble(L, 3, &count, 1);
      if (count != 2)
      {
        free(value);
        luaL_argerror(L, 3, "invalid value");
      }

      cdata[2*index] = value[0];
      cdata[2*index+1] = value[1];
      free(value);
    }
    break;
  }

  return 0;
}

/*****************************************************************************\
 image channel indexing
\*****************************************************************************/
static int imluaImageChannel_index (lua_State *L)
{
  imluaImageChannel *image_channel = imlua_checkimagechannel(L, 1);
  int lin = luaL_checkinteger(L, 2);

  if (lin < 0 || lin >= image_channel->image->height)
    luaL_argerror(L, 2, "invalid lin, out of bounds");

  imlua_newimageline(L, image_channel->image, image_channel->channel, lin);
  return 1;
}

/*****************************************************************************\
 image indexing
\*****************************************************************************/
static int imluaImage_index (lua_State *L)
{
  imImage *image = imlua_checkimage(L, 1);

  if (lua_isnumber(L, 2))
  {
    /* handle numeric indexing */
    int channel = luaL_checkinteger(L, 2);

    /* create channel */
    int depth = image->has_alpha? image->depth+1: image->depth;
    if (channel < 0 || channel >= depth)
      luaL_argerror(L, 2, "invalid channel, out of bounds");

    imlua_newimagechannel(L, image, channel);
  }
  else if (lua_isstring(L, 2))
  {
    /* get raw method */
    lua_getmetatable(L, 1);
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);
  }
  else
  {
    lua_pushnil(L);
  }

  return 1;
}

static const luaL_Reg imimage_lib[] = {
  {"ImageCreate", imluaImageCreate},
  {"ImageCreateFromOpenGLData", imluaImageCreateFromOpenGLData},
  {"ImageDestroy", imluaImageDestroy},
  {"FileImageLoad", imluaFileImageLoad},
  {"FileImageLoadBitmap", imluaFileImageLoadBitmap},
  {"FileImageLoadRegion", imluaFileImageLoadRegion},
  {"FileImageSave", imluaFileImageSave},
  {NULL, NULL}
};

static const luaL_Reg imimage_metalib[] = {
  {"Destroy", imluaImageDestroy},
  {"SetPixels", imluaImageSetPixels},
  {"GetPixels", imluaImageGetPixels},
  {"AddAlpha", imluaImageAddAlpha},
  {"RemoveAlpha", imluaImageRemoveAlpha},
  {"SetAlpha", imluaImageSetAlpha},
  {"Reshape", imluaImageReshape},
  {"Copy", imluaImageCopy},
  {"CopyData", imluaImageCopyData},
  {"CopyPlane", imluaImageCopyPlane},
  {"Duplicate", imluaImageDuplicate},
  {"Clone", imluaImageClone},
  {"SetAttribute", imluaImageSetAttribute},
  {"SetAttribInteger", imluaImageSetAttribInteger},
  {"SetAttribReal", imluaImageSetAttribReal},
  {"SetAttribString", imluaImageSetAttribString},
  {"GetAttribute", imluaImageGetAttribute},
  {"GetAttribInteger", imluaImageGetAttribInteger},
  {"GetAttribReal", imluaImageGetAttribReal},
  {"GetAttribString", imluaImageGetAttribString},
  {"GetAttributeList", imluaImageGetAttributeList},
  {"Clear", imluaImageClear},
  {"IsBitmap", imluaImageIsBitmap},
  {"GetOpenGLData", imluaImageGetOpenGLData},
  {"SetPalette", imluaImageSetPalette},
  {"GetPalette", imluaImageGetPalette},
  {"CopyAttributes", imluaImageCopyAttributes},
  {"MergeAttributes", imluaImageMergeAttributes},
  {"MatchSize", imluaImageMatchSize},
  {"MatchColor", imluaImageMatchColor},
  {"MatchDataType", imluaImageMatchDataType},
  {"MatchColorSpace", imluaImageMatchColorSpace},
  {"Match", imluaImageMatch},
  {"SetBinary", imluaImageSetBinary},
  {"SetMap", imluaImageSetMap},
  {"SetGray", imluaImageSetGray},
  {"MakeBinary", imluaImageMakeBinary},
  {"MakeGray", imluaImageMakeGray},
  {"Width", imluaImageWidth},
  {"Height", imluaImageHeight},
  {"Depth", imluaImageDepth},
  {"DataType", imluaImageDataType},
  {"ColorSpace", imluaImageColorSpace},
  {"HasAlpha", imluaImageHasAlpha},
  {"Save", imluaImageSave},

  {"__gc", imluaImage_gc},
  {"__tostring", imluaImage_tostring},
  {"__index", imluaImage_index},

  {NULL, NULL}
};

static void createmeta (lua_State *L) 
{
  /* image[plane][lin][column] */

  luaL_newmetatable(L, "imImageChannel"); /* create new metatable for imImageChannel handles */
  lua_pushliteral(L, "__index");
  lua_pushcfunction(L, imluaImageChannel_index);
  lua_rawset(L, -3);
  lua_pushliteral(L, "__tostring");
  lua_pushcfunction(L, imluaImageChannel_tostring);
  lua_rawset(L, -3);
  lua_pop(L, 1);  /* removes the metatable from the top of the stack */

  luaL_newmetatable(L, "imImageChannelLine"); /* create new metatable for imImageChannelLine handles */
  lua_pushliteral(L, "__index");
  lua_pushcfunction(L, imluaImageLine_index);
  lua_rawset(L, -3);
  lua_pushliteral(L, "__newindex");
  lua_pushcfunction(L, imluaImageLine_newindex);
  lua_rawset(L, -3);
  lua_pushliteral(L, "__tostring");
  lua_pushcfunction(L, imluaImageLine_tostring);
  lua_rawset(L, -3);
  lua_pop(L, 1);   /* removes the metatable from the top of the stack */

  /* Object Oriented Access */
  luaL_newmetatable(L, "imImage");  /* create new metatable for imImage handles */
  lua_pushliteral(L, "__index");    /* dummy code because imluaImage_index will overwrite this behavior */
  lua_pushvalue(L, -2);  /* push metatable */
  lua_rawset(L, -3);  /* metatable.__index = metatable */
  luaL_register(L, NULL, imimage_metalib);  /* register methods */
  lua_pop(L, 1);  /* removes the metatable from the top of the stack */
}

void imlua_open_image (lua_State *L)
{
  /* "im" table is at the top of the stack */
  createmeta(L);
  luaL_register(L, NULL, imimage_lib);

#ifdef IMLUA_USELOH
#include "im_image.loh"
#else
#ifdef IMLUA_USELH
#include "im_image.lh"
#else
  luaL_dofile(L, "im_image.lua");
#endif
#endif

}
