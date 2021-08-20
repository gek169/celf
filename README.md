# CELF, the custom Executable-Linkable format

Fully public domain ELF-like format for executables.

Designed for OSdev projects in desparate need of a cross compiler.

Theoretically portable to any 32 or 64 bit architecture.

Cross compilation can occur from any 64 bit architecture to any 32 bit one, or betwee 32 bit architectures,

but the code will need a minor re-write (from size_t to unsigned long long or uint64_t) to compile code from a 32 bit arch to a 64 bit one.

# INTEGRATION

Your cross-compiler will not implement link, make_exec, make_ro, or any of that, it will only use the serialization path.




Your operating system's kernel needs to provide implementations for malloc, make_ro, make_exec, and link.

Before the ELF is executed, make_ro is called on rodata, make_exec is called on execd (think: like .text for x86)

and celf_link is called on the entire data structure.


the actions of celf_link are system and architecture specific, your compiler will ultimately have its own way of outputting statically linked symbols.

If you don't understand what I mean...

Your compiler will probably output a series of string references, I.E. "Pointer to MyGlobalVariable"

These statically linked symbols need to be resolved to pointers in memory, at runtime.

I have no idea where your compiler stores these tags, what their format is, or how to resolve the tags.

So I wrote a function that resolves a symbol to a pointer and it's your responsibility, as the os developer, to correctly parse
the output of your tags.

C++-ic pseudo-code:
```c
for(Tag& tag:tags){
	celf_entry* entry = celf_resolve_symbol(my_celf_file, tag.name);
	if(entry == CELF_MAGIC_INVALID_PTR) goto error;
	tag_resolve(tag, entry); //Modify the tag to use this statically linked pointer rather than the tag.
}
```
If your architecture uses fixed addressing (I.E. rodata will always be at some specific location in memory...) then this isn't needed.

You can make dummy entries (see below)

since your compiler will compile direct pointers and no symbol resolution needs to be done.


If your compiler outputs section-relative pointers, you can make dummy entries like this:

entry: myRodata 
offset: 0
section: rodata
size: (the size of your rodata...)

entry: myExecd
offset: 0
section: execd
size: (the size of your executable data...)

Then replace all references/tags to the section names to their pointers.

This whole system really shines when you want to do *dynamic linking* though.

When you want to do dynamic linking, you will definitely have symbol names which need to be resolved to pointers.

With celf, you can easily check all loaded ELFs in the current process, and get the dynamically linked symbol.


