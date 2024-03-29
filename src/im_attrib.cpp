/** \file
 * \brief Attributes Table
 *
 * See Copyright Notice in im_lib.h
 */

#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <assert.h>

#include "im.h"
#include "im_attrib.h"
#include "im_util.h"

#define IM_DEFAULTSIZE 101
#define IM_MULTIPLIER 31

// Unique Hash index for a name
static int iHashIndex(const char *name, int hash_size)
{
  unsigned short hash = 0;
  const unsigned char *p_name = (const unsigned char*)name;

  for(; *p_name; p_name++)
    hash = hash*IM_MULTIPLIER + *p_name;

  return hash % hash_size;
}


/*******************************************************************/


class imAttribNode
{
public:
  int data_type;
  int count;
  void* data;
  char* name;

  imAttribNode* next;

  imAttribNode(const char* name, int _data_type, int _count, const void* _data, imAttribNode* next);
  ~imAttribNode();
};

static char* utlStrDup(const char* str)
{
  int size;
  char* new_str;

  assert(str);

  size = (int)strlen(str) + 1;
  new_str = (char*)malloc(size);
  memcpy(new_str, str, size);

  return new_str;
}

imAttribNode::imAttribNode(const char* name, int _data_type, int _count, const void* _data, imAttribNode* _next)
{
  if (_data_type == 0 && _count == -1)  /* BYTE meaning a string */
    _count = (int)strlen((char*)_data)+1;

  this->name = utlStrDup(name);
  this->data_type = _data_type;
  this->count = _count;
  this->next = _next;

  int size = _count * imDataTypeSize(_data_type);
  this->data = malloc(size);
  if (_data) memcpy(this->data, _data, size);
  else memset(this->data, 0, size);
}

imAttribNode::~imAttribNode()
{
  free(this->name); 
  free(this->data);
}


/*******************************************************************/

struct imAttribTablePrivate
{
  int count,       
      hash_size;   
  imAttribNode* *hash_table;
};

imAttribTablePrivate* imAttribTableCreate(int hash_size)
{
  imAttribTablePrivate* ptable = (imAttribTablePrivate*)malloc(sizeof(imAttribTablePrivate));
  ptable->count = 0;
  ptable->hash_size = (hash_size == 0)? IM_DEFAULTSIZE: hash_size;
  ptable->hash_table = (imAttribNode**)malloc(ptable->hash_size*sizeof(imAttribNode*));
  memset(ptable->hash_table, 0, ptable->hash_size*sizeof(imAttribNode*));
  return ptable;
}

imAttribTablePrivate* imAttribArrayCreate(int count)
{
  imAttribTablePrivate* ptable = (imAttribTablePrivate*)malloc(sizeof(imAttribTablePrivate));
  ptable->hash_size = ptable->count = count;
  ptable->hash_table = (imAttribNode**)malloc(ptable->count*sizeof(imAttribNode*));
  memset(ptable->hash_table, 0, ptable->hash_size*sizeof(imAttribNode*));
  return ptable;
}

void imAttribTableDestroy(imAttribTablePrivate* ptable)
{
  imAttribTableRemoveAll(ptable);
  free(ptable->hash_table);
  free(ptable);
}

int imAttribTableCount(imAttribTablePrivate* ptable)
{
  return ptable->count;
}

void imAttribTableRemoveAll(imAttribTablePrivate* ptable)
{
  if (ptable->count == 0) return;

  int n = 0;
  for(int i = 0; i < ptable->hash_size; i++) 
  {
    imAttribNode* cur_node = ptable->hash_table[i];
    while (cur_node) 
    {
      imAttribNode* next_node = cur_node->next;
      delete cur_node;
      cur_node = next_node;
      n++;
    }

    ptable->hash_table[i] = NULL;

    if (n == ptable->count)
      break;
  }
  
  ptable->count = 0;
}

void imAttribTableSet(imAttribTablePrivate* ptable, const char* name, int data_type, int count, const void* data)
{
  assert(name);

  int index = iHashIndex(name, ptable->hash_size);
  imAttribNode* first_node = ptable->hash_table[index];

  // The name already exists ?
  imAttribNode* cur_node = first_node;
  imAttribNode* prev_node = NULL;
  while (cur_node) 
  {
    if (imStrEqual(cur_node->name, name))
    {
      // Found, replace current node.
      imAttribNode* new_node = new imAttribNode(name, data_type, count, data, cur_node->next);

      // Is first node ?
      if (cur_node == first_node)
        ptable->hash_table[index] = new_node;
      else
        prev_node->next = new_node;

      delete cur_node;
      return;
    }

    prev_node = cur_node;
    cur_node = cur_node->next;
  }

  // Not found, the new item goes first.
  cur_node = new imAttribNode(name, data_type, count, data, first_node);
  ptable->hash_table[index] = cur_node;
	ptable->count++;
}

void imAttribTableUnSet(imAttribTablePrivate* ptable, const char *name)
{
  assert(name);

  if (ptable->count == 0) return;

  int index = iHashIndex(name, ptable->hash_size);

  imAttribNode* cur_node = ptable->hash_table[index];
  imAttribNode* prev_node = cur_node;
  while (cur_node) 
  {
    if (imStrEqual(cur_node->name, name))
    {
      // Is first node ?
      if (cur_node == prev_node)
        ptable->hash_table[index] = cur_node->next;
      else
        prev_node->next = cur_node->next;

      delete cur_node;
      ptable->count--;
      return;
    }

    prev_node = cur_node;
    cur_node = cur_node->next;
  }
}

const void* imAttribTableGet(const imAttribTablePrivate* ptable, const char *name, int *data_type, int *count)
{
  assert(name);

  if (ptable->count == 0) return NULL;

  int index = iHashIndex(name, ptable->hash_size);

  imAttribNode* cur_node = ptable->hash_table[index];
  while (cur_node) 
  {
    if (imStrEqual(cur_node->name, name))
    {
      if (data_type) *data_type = cur_node->data_type;
      if (count) *count = cur_node->count;
      return cur_node->data;
    }

    cur_node = cur_node->next;
  }

  return NULL;
}

void imAttribArraySet(imAttribTablePrivate* ptable, int index, const char* name, int data_type, int count, const void* data)
{
  assert(name);
  assert(index < ptable->count);

  if (index >= ptable->count) return;

  imAttribNode* node = ptable->hash_table[index];
  if (node) delete node;

  ptable->hash_table[index] = new imAttribNode(name, data_type, count, data, NULL);
}

const void* imAttribArrayGet(const imAttribTablePrivate* ptable, int index, char *name, int *data_type, int *count)
{
  if (ptable->count == 0) return NULL;

  imAttribNode* node = ptable->hash_table[index];
  if (node) 
  {
    if (name) strcpy(name, node->name);
    if (data_type) *data_type = node->data_type;
    if (count) *count = node->count;
    return node->data;
  }

  return NULL;
}

void imAttribTableForEach(const imAttribTablePrivate* ptable, void* user_data, imAttribTableCallback attrib_func)
{
  assert(attrib_func);

  if (ptable->count == 0) return;

  int index = 0;
  for(int i = 0; i < ptable->hash_size; i++) 
  {
    imAttribNode* cur_node = ptable->hash_table[i];
    while (cur_node) 
    {
      if (!attrib_func(user_data, index, cur_node->name, cur_node->data_type, cur_node->count, cur_node->data))
        return;

      index++;
      cur_node = cur_node->next;
    }

    if (index == ptable->count)
      return;
  }
}

static int iCopyFunc(void* user_data, int index, const char* name, int data_type, int count, const void* data)
{                  
  (void)index;
  imAttribTablePrivate* ptable = (imAttribTablePrivate*)user_data;
  imAttribTableSet(ptable, name, data_type, count, data);
  return 1;
}

void imAttribTableCopyFrom(imAttribTablePrivate* ptable_dst, const imAttribTablePrivate* ptable_src)
{
  imAttribTableForEach(ptable_src, (void*)ptable_dst, iCopyFunc);
}

static int iMergeFunc(void* user_data, int index, const char* name, int data_type, int count, const void* data)
{                  
  (void)index;
  imAttribTablePrivate* ptable = (imAttribTablePrivate*)user_data;
  if (!imAttribTableGet(ptable, name, NULL, NULL))
    imAttribTableSet(ptable, name, data_type, count, data);
  return 1;
}

void imAttribTableMergeFrom(imAttribTablePrivate* ptable_dst, const imAttribTablePrivate* ptable_src)
{
  imAttribTableForEach(ptable_src, (void*)ptable_dst, iMergeFunc);
}

static int iCopyArrayFunc(void* user_data, int index, const char* name, int data_type, int count, const void* data)
{                  
  (void)index;
  imAttribTablePrivate* ptable = (imAttribTablePrivate*)user_data;
  imAttribArraySet(ptable, index, name, data_type, count, data);
  return 1;
}

void imAttribArrayCopyFrom(imAttribTablePrivate* ptable_dst, const imAttribTablePrivate* ptable_src)
{
  imAttribTableForEach(ptable_src, (void*)ptable_dst, iCopyArrayFunc);
}

int imAttribTableGetInteger(imAttribTablePrivate* ptable, const char *name, int index)
{
  int data_type, count;
  const void* data = imAttribTableGet(ptable, name, &data_type, &count);
  if (!data || index < 0 || index >= count) 
    return 0;

  switch (data_type)
  {
  case IM_BYTE:
    return (int)((imbyte*)data)[index];
  case IM_SHORT:
    return (int)((short*)data)[index];
  case IM_USHORT:
    return (int)((imushort*)data)[index];
  case IM_INT:
    return (int)((int*)data)[index];
  case IM_FLOAT:
    return (int)((float*)data)[index];
  case IM_DOUBLE:
    return (int)((double*)data)[index];
  case IM_CFLOAT:
  case IM_CDOUBLE:
    return 0;
  }
  return 0;
}

double imAttribTableGetReal(imAttribTablePrivate* ptable, const char *name, int index)
{
  int data_type, count;
  const void* data = imAttribTableGet(ptable, name, &data_type, &count);
  if (!data || index < 0 || index >= count) 
    return 0;

  switch (data_type)
  {
  case IM_BYTE:
    return (double)((imbyte*)data)[index];
  case IM_SHORT:
    return (double)((short*)data)[index];
  case IM_USHORT:
    return (double)((imushort*)data)[index];
  case IM_INT:
    return (double)((int*)data)[index];
  case IM_FLOAT:
    return (double)((float*)data)[index];
  case IM_DOUBLE:
    return (double)((double*)data)[index];
  case IM_CFLOAT:
  case IM_CDOUBLE:
    return 0;
  }
  return 0;
}

static int iFindZero(imbyte* data, int count)
{
  while (count)
  {
    if (*data == 0)
      return 1;
    count--;
    data++;
  }
  return 0;
}

const char* imAttribTableGetString(imAttribTablePrivate* ptable, const char *name)
{
  int data_type, count;
  const void* data = imAttribTableGet(ptable, name, &data_type, &count);
  if (!data || data_type != IM_BYTE || !iFindZero((imbyte*)data, count)) 
    return 0;
  return (const char*)data;
}

void imAttribTableSetInteger(imAttribTablePrivate* ptable, const char* name, int data_type, int value)
{
  switch (data_type)
  {
  case IM_BYTE:
    {
      imbyte data = (imbyte)value;
      imAttribTableSet(ptable, name, data_type, 1, (void*)&data);
      break;
    }
  case IM_SHORT:
    {
      short data = (short)value;
      imAttribTableSet(ptable, name, data_type, 1, (void*)&data);
      break;
    }
  case IM_USHORT:
    {
      imushort data = (imushort)value;
      imAttribTableSet(ptable, name, data_type, 1, (void*)&data);
      break;
    }
  case IM_INT:
    {
      int data = (int)value;
      imAttribTableSet(ptable, name, data_type, 1, (void*)&data);
      break;
    }
  case IM_FLOAT:
    {
      float data = (float)value;
      imAttribTableSet(ptable, name, data_type, 1, (void*)&data);
      break;
    }
  case IM_DOUBLE:
    {
      double data = (double)value;
      imAttribTableSet(ptable, name, data_type, 1, (void*)&data);
      break;
    }
  case IM_CFLOAT:
  case IM_CDOUBLE:
    break;
  }
}

void imAttribTableSetReal(imAttribTablePrivate* ptable, const char* name, int data_type, double value)
{
  switch (data_type)
  {
  case IM_BYTE:
  {
    imbyte data = (imbyte)value;
    imAttribTableSet(ptable, name, data_type, 1, (void*)&data);
    break;
  }
  case IM_SHORT:
  {
    short data = (short)value;
    imAttribTableSet(ptable, name, data_type, 1, (void*)&data);
    break;
  }
  case IM_USHORT:
  {
    imushort data = (imushort)value;
    imAttribTableSet(ptable, name, data_type, 1, (void*)&data);
    break;
  }
  case IM_INT:
  {
    int data = (int)value;
    imAttribTableSet(ptable, name, data_type, 1, (void*)&data);
    break;
  }
  case IM_FLOAT:
  {
    float data = (float)value;
    imAttribTableSet(ptable, name, data_type, 1, (void*)&data);
    break;
  }
  case IM_DOUBLE:
  {
    double data = (double)value;
    imAttribTableSet(ptable, name, data_type, 1, (void*)&data);
    break;
  }
  case IM_CFLOAT:
  case IM_CDOUBLE:
    break;
  }
}

void imAttribTableSetString(imAttribTablePrivate* ptable, const char* name, const char* value)
{
  assert(value);

  int count = (int)strlen(value) + 1;
  imAttribTableSet(ptable, name, IM_BYTE, count, (void*)value);
}
