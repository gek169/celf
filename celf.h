#include <stddef.h>

/*
	CELF- the Custom Executable-Linkable Format

	IMPLEMENTATION-

	you must implement some functions.

	Make a piece of memory executable:
		void celf_make_exec(void* blk, size_t sz);
	Allocate memory with the maximum alignment:
		void* celf_malloc(size_t sz);
	Make a piece of memory read-only:
		void celf_make_ro(void* blk, size_t sz);
	Display some sort of error message:
		void celf_err(const char* errmsg);
	Rebuild execd based on references to items sections
		void celf_link(celf_file* data);
	

	the celf_malloc function must return an amount of memory which is aligned precisely to CELF_ALIGNMENT.

	All entries are aligned to CELF_NEEDED_ALIGNMENT.

	USAGE- 

	You must define some constants for your system.

	

	Creating a celf:

	Pass in a set of celf_entry_ins in an array,

	the celf file struct will be created, which can be used or serialized.

	To serialize, pass that same struct into the serializer.

	The resulting block must be written to disk.


	Loading an elf:

	The file is loaded onto disk somewhere in RW memory.

	This is passed into a function which parses it into the celf file struct.
	

*/
/*
	The needed alignment by the system.
*/

#define CELF_ALIGNMENT 128

#ifdef CELF_64
#define CELF_UINT_SIZE 8
#else
#define CELF_UINT_SIZE 4
#endif


#define CELF_RODATA 0
#define CELF_UDATA 1
#define CELF_ZDATA 2
#define CELF_IDATA 3
#define CELF_EXECD 4


#ifdef CELF_USE_STRING

#include <string.h>
#define celf_memset memset
#define celf_memcpy memcpy
#else
void celf_memset(void* strin, int c, size_t n){
	unsigned char* str = strin;
	for(;n;n--,str++) *str = c;
}
void celf_memcpy(void* restrict destination, void* restrict source, size_t nbytes){
	unsigned char* d = destination;
	unsigned char* s = source;
	while(nbytes--) *d++ = *s++;
}
#endif
/*
	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	FUNCTIONS THAT MUST BE IMPLEMENTED
	~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/
/*
	Allocate size sz, with the maximum useful alignment.
*/
void* celf_malloc(size_t sz);
void celf_free(void* p);
#ifdef CELF_USE_MALLOC
#include <stdlib.h>
void* celf_malloc(size_t sz){return malloc(sz);}
void celf_free(void* p){
	if(p) free(p);
}
#endif
/*
	make block executable.
*/
void celf_make_exec(void* blk, size_t sz);
/*
	Make block read-only
*/
void celf_make_ro(void* blk, size_t sz);

#ifndef CELF_USE_MPROTECT
#ifdef CELF_DUMMY_MAKE_RO
void celf_make_ro(void* blk, size_t sz){
	(void)blk;
	(void)sz;
}
#endif
#endif

/*
	Print an error message
*/
void celf_err(const char* errmsg);
#ifdef CELF_DEFAULT_ERR
#include <stdio.h>
#include <stdlib.h>
void celf_err(const char* errmsg){
	puts(errmsg);
	abort();
}
#endif




#ifdef CELF_DUMMY_link
void celf_link(celf_file* data){
	(void)data;
}
#endif

#ifdef CELF_USE_MPROTECT
#include <sys/mman.h>
#include <unistd.h>
void celf_make_exec(void* blk, size_t sz){
	unsigned char* blk_mod = blk;
	/*Must be aligned to the block.*/
	blk_mod = blk_mod - ((size_t)blk % getpagesize());
	mprotect(blk_mod, sz, PROT_READ | PROT_EXEC);
}

void celf_make_ro(void* blk, size_t sz){
	unsigned char* blk_mod = blk;
	/*Must be aligned to the block.*/
	blk_mod = blk_mod - ((size_t)blk % getpagesize());
	mprotect(blk_mod, sz, PROT_READ);
}
#endif




static size_t celf_parse_uint(void* where){
	size_t retval = 0;
	unsigned char* b = where;
	retval |= *b; retval <<= 8; b++;
	retval |= *b; retval <<= 8; b++;
	retval |= *b; retval <<= 8; b++;
	retval |= *b; 

#ifdef CELF_64
	retval <<= 8; b++;
	retval |= *b; retval <<= 8; b++;
	retval |= *b; retval <<= 8; b++;
	retval |= *b; retval <<= 8; b++;
	retval |= *b; 
#endif
	return retval;
}

static void celf_write_uint(void* where, size_t val){
	unsigned char* b = where;
#ifdef CELF_64
	*b = val >> 56; b++;
	*b = val >> 48; b++;
	*b = val >> 40; b++;
	*b = val >> 32; b++;
#endif	
	*b = val >> 24; b++;
	*b = val >> 16; b++;
	*b = val >> 8 ; b++;
	*b = val      ;
}


typedef struct{
	unsigned char entry_section;
	size_t entry_offset; /*Where in the data?*/
	char entry_name[256]; /*Name. Must not be */
} celf_entry;

typedef struct{
	char entry_name[256];
	unsigned char entry_section;
	void* entry_data;
	size_t entry_size;
} celf_entry_in;

typedef struct{
	celf_entry* entries; /**/
	size_t nentries; /**/
	void* rodata; /*Read-Only Data*/
	size_t rodata_size;
	void* udata;
	size_t udata_size;
	void* zdata;
	size_t zdata_size;
	void* idata;
	size_t idata_size;
	void* execd; /*Executable, read-only data.*/
	size_t execd_size;
} celf_file;


/*
	Modify Execd to utilize references in sections.

	You might also want to modify other sections.

	The format of these references is ultimately... system specific.

	So it is not portable to write them here.
*/
void celf_link(celf_file* data);


static int celf_streq_256(const char* a, const char* b){
	int i;
	for(i = 0; i < 256; i++)
	{
		if(*a != *b) return 0;
		if(*a == '\0') return 1;
		a++;b++;
	}
	return 1; /*First 256 characters are matching.*/
}

#define CELF_MAGIC_INVALID_PTR NULL



static void* celf_resolve_symbol(celf_file* f, const char* sym){
	size_t i = 0;
	for(i = 0; i < f->nentries; i++){
		if(celf_streq_256(sym, f->entries[i].entry_name)){
			switch(f->entries[i].entry_section){
				default: return NULL;
				case CELF_RODATA: return ((unsigned char*)f->rodata) + f->entries[i].entry_offset;
				case CELF_IDATA: return ((unsigned char*)f->idata) + f->entries[i].entry_offset;
				case CELF_ZDATA: return ((unsigned char*)f->zdata) + f->entries[i].entry_offset;
				case CELF_UDATA: return ((unsigned char*)f->udata) + f->entries[i].entry_offset;
				case CELF_EXECD: return ((unsigned char*)f->execd) + f->entries[i].entry_offset;
			}
		}
	}
	return CELF_MAGIC_INVALID_PTR;
}


static celf_file celf_deserialize (void* bufin, size_t bufsz){
	celf_file retval = {0};
	size_t rodata_size = 0;
	size_t udata_size = 0;
	size_t zdata_size = 0;
	size_t idata_size = 0;
	size_t execd_size = 0;
	size_t nentries = 0;
	unsigned char* buf = bufin;
	size_t size_needed;

	if(bufsz < 6 * CELF_UINT_SIZE) goto error2;
	nentries = celf_parse_uint(buf); 	buf += CELF_UINT_SIZE;
	if(nentries == 0) return retval; /*Don't even proceed. Malformed.*/
	rodata_size = celf_parse_uint(buf); buf += CELF_UINT_SIZE;
	udata_size = celf_parse_uint(buf); 	buf += CELF_UINT_SIZE;
	zdata_size = celf_parse_uint(buf); 	buf += CELF_UINT_SIZE;
	idata_size = celf_parse_uint(buf); 	buf += CELF_UINT_SIZE;
	execd_size = celf_parse_uint(buf); 	buf += CELF_UINT_SIZE;

	size_needed	 = rodata_size + idata_size + execd_size + 
	nentries * (256 + 1 + CELF_UINT_SIZE)  /*256 bytes for name, UINT_SIZE bytes for section offset, 1 byte for section ID.*/
	+ 6 * CELF_UINT_SIZE; /*6 integers are stored.*/
	/*Uh oh!*/
	if(size_needed < bufsz) goto error2;
	/*Allocate memory for sections.*/
	retval.entries = celf_malloc(sizeof(celf_entry) * nentries); 
	if(retval.entries == NULL) goto error;
	retval.nentries = nentries;
	/*Fill entries with data.*/
	{size_t i = 0;for(;i < retval.nentries; i++){
		retval.entries[i].entry_section = *buf;					buf++;
		retval.entries[i].entry_offset = celf_parse_uint(buf); 	buf += CELF_UINT_SIZE;
		/*TODO: validate entry_offset.*/
		celf_memcpy(retval.entries[i].entry_name, buf, 256); 	buf += 256;
	}}
	retval.rodata = celf_malloc(rodata_size); 	retval.rodata_size = rodata_size; if(retval.rodata == NULL) goto error;
	retval.udata = celf_malloc(udata_size); 	retval.udata_size = udata_size; if(retval.udata == NULL) goto error;
	retval.idata = celf_malloc(idata_size); 	retval.idata_size = idata_size; if(retval.idata == NULL) goto error;
	retval.zdata = celf_malloc(zdata_size); 	retval.zdata_size = zdata_size; if(retval.zdata == NULL) goto error;
	retval.execd = celf_malloc(execd_size); 	retval.execd_size = execd_size; if(retval.execd == NULL) goto error;
	/* Copy section data.*/
	celf_memset(retval.zdata, 0, zdata_size);
	celf_memcpy(retval.rodata, buf, rodata_size);	buf += rodata_size;
	celf_memcpy(retval.idata, buf, idata_size);		buf += idata_size;
	celf_memcpy(retval.execd, buf, execd_size);		/*buf += execd_size;*/
	celf_link(&retval);
	celf_make_exec(retval.execd, execd_size);
	celf_make_ro(retval.rodata, rodata_size);
	return retval;

	error:
	{
		static const char* malloc_fail_err_msg = "failed malloc in celf_deserialize";
		celf_err(malloc_fail_err_msg);
		return retval;
	}
	error2:
	{
		static const char* fail_err_msg = "Buffer was too small for celf_deserialize, most likely a malformed file.";
		celf_err(fail_err_msg);
		return retval;
	}
}

/*
	Prepare celf data for serialization.
*/
static void* celf_serialize(celf_file* d, size_t* needed_out){
	static const char* malloc_fail_err_msg = "failed malloc in celf_serialize";
	void* retval = NULL;
	unsigned char* r;
	size_t size_needed = d->rodata_size + d->idata_size + d->execd_size + d->nentries * 
	(256 + 1 + CELF_UINT_SIZE)  /*256 bytes for name, UINT_SIZE bytes for section offset, 1 byte for section ID.*/
	+ 6 * CELF_UINT_SIZE; /*6 integers are stored.*/

	if(needed_out) *needed_out = size_needed;
	retval = celf_malloc(size_needed);
	r = retval;
	if(!retval) {celf_err(malloc_fail_err_msg);	return NULL;}

	/*First, write the six sizes. The six integers.*/
	celf_write_uint(r, d->nentries); r+= CELF_UINT_SIZE;
	celf_write_uint(r, d->rodata_size); r+= CELF_UINT_SIZE;
	celf_write_uint(r, d->udata_size);r+= CELF_UINT_SIZE;
	
	celf_write_uint(r, d->zdata_size);r+= CELF_UINT_SIZE;
	celf_write_uint(r, d->idata_size);r+= CELF_UINT_SIZE;
	celf_write_uint(r, d->execd_size);r+= CELF_UINT_SIZE;
	/*Second, write entries.*/
	{size_t i = 0;for(; i < d->nentries; i++){
		*r = d->entries[i].entry_section; r++;
		celf_write_uint(r, d->entries[i].entry_offset); 
		r+= CELF_UINT_SIZE;
		celf_memcpy(r, d->entries[i].entry_name, 256);
		r += 256;
		
	}}
	/*Now, write the three sections with data..*/
	if(d->rodata && d->rodata_size)
	{
		celf_memcpy(r, d->rodata, d->rodata_size);
		r += d->rodata_size;
	}
	if(d->idata && d->idata_size)
	{
		celf_memcpy(r, d->idata, d->idata_size);
		r += d->idata_size;
	}
	if(d->execd && d->execd_size)
	{
		celf_memcpy(r, d->execd, d->execd_size);
		r += d->execd_size;
	}
	return retval;
}

/*
	From a list of entries, generate the needed data sizes and build the file structure.

	NOTE!!!! You cannot directly use the output of this its only purpose is to pass into the serializer.

	the udata and zdata are NULL because they aren't needed for serialization.
*/	
static celf_file celf_build_filestruct(celf_entry_in* entries, size_t nentries){
	static const char* malloc_fail_err_msg = "failed malloc in celf_build_filestruct";
	static const char* sectionid_fail_err_msg = "improper section ID.";
	celf_file retval = {0};
	size_t rodata_size = 0;
	size_t udata_size = 0;
	size_t zdata_size = 0;
	size_t idata_size = 0;
	size_t execd_size = 0;
	/*Keep track of our progress writing entries in.*/
	size_t rodata_prog = 0;
	size_t udata_prog = 0;
	size_t zdata_prog = 0;
	size_t idata_prog = 0;
	size_t execd_prog = 0;
	/*Loop over all entries.*/
	{size_t i;for(i = 0; i < nentries; i++){
		size_t needed = (entries[i].entry_size + CELF_ALIGNMENT-1);
		needed /= CELF_ALIGNMENT;
		needed *= CELF_ALIGNMENT;
		switch(entries[i].entry_section){
			case 0:rodata_size += needed;break;
			case 1:udata_size += needed;break;
			case 2:zdata_size += needed;break;
			case 3:idata_size += needed;break;
			case 4:execd_size += needed;break;
			default: {celf_err(sectionid_fail_err_msg);return retval;}
		}
	}}
	/*Allocate memory for each of the sections.*/
	retval.entries = celf_malloc(sizeof(celf_entry) * nentries); if(retval.entries == NULL) {celf_err(malloc_fail_err_msg);return retval;}

	retval.rodata = celf_malloc(rodata_size); if(retval.rodata == NULL)						{celf_err(malloc_fail_err_msg);return retval;}
/*	retval.udata = celf_malloc(udata_size); if(retval.udata == NULL) 						{celf_err(malloc_fail_err_msg);return retval;}*/
	retval.idata = celf_malloc(idata_size); if(retval.idata == NULL) 						{celf_err(malloc_fail_err_msg);return retval;}
	/*retval.zdata = celf_malloc(zdata_size); if(retval.zdata == NULL) 						{celf_err(malloc_fail_err_msg);return retval;}*/
	retval.execd = celf_malloc(execd_size); if(retval.execd == NULL) 						{celf_err(malloc_fail_err_msg);return retval;}
	/*memset the Zdata.*/
	celf_memset(retval.zdata, 0, zdata_size);
	/*Write metadata*/
	retval.nentries = nentries;
	retval.rodata_size = rodata_size;
	retval.udata_size = udata_size;
	retval.idata_size = idata_size;
	retval.zdata_size = zdata_size;
	retval.execd_size = execd_size;
	
	/*Loop over all entries again, this time to write them into the allocated buffers.*/
		{size_t i;for(i = 0; i < nentries; i++){
			size_t needed = (entries[i].entry_size + CELF_ALIGNMENT-1);
			needed /= CELF_ALIGNMENT;
			needed *= CELF_ALIGNMENT;
			retval.entries[i].entry_section = entries[i].entry_section;
			celf_memcpy(retval.entries[i].entry_name, entries[i].entry_name, 256);
			switch(entries[i].entry_section){
				case 0:
					retval.entries[i].entry_offset = rodata_prog;  
					celf_memcpy(retval.rodata, entries[i].entry_data, entries[i].entry_size);
					rodata_prog += needed;
				break;
				/*Not needed.*/
				case 1:retval.entries[i].entry_offset = udata_prog;  udata_prog += needed;break;
				case 2:retval.entries[i].entry_offset = zdata_prog;  zdata_prog += needed;break;

				case 3:retval.entries[i].entry_offset = idata_prog;  
					celf_memcpy(retval.idata, entries[i].entry_data, entries[i].entry_size);
					idata_prog += needed;
				break;
				case 4:retval.entries[i].entry_offset = execd_prog;  
					celf_memcpy(retval.execd, entries[i].entry_data, entries[i].entry_size);
					execd_prog += needed;
				break;
			}
		}}
	return retval;
}

