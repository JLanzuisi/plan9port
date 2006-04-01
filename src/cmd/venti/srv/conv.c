#include "stdinc.h"
#include "dat.h"
#include "fns.h"

/*
 * disk structure conversion routines
 */
#define	U8GET(p)	((p)[0])
#define	U16GET(p)	(((p)[0]<<8)|(p)[1])
#define	U32GET(p)	((u32int)(((p)[0]<<24)|((p)[1]<<16)|((p)[2]<<8)|(p)[3]))
#define	U64GET(p)	(((u64int)U32GET(p)<<32)|(u64int)U32GET((p)+4))

#define	U8PUT(p,v)	(p)[0]=(v)&0xFF
#define	U16PUT(p,v)	(p)[0]=((v)>>8)&0xFF;(p)[1]=(v)&0xFF
#define	U32PUT(p,v)	(p)[0]=((v)>>24)&0xFF;(p)[1]=((v)>>16)&0xFF;(p)[2]=((v)>>8)&0xFF;(p)[3]=(v)&0xFF
#define	U64PUT(p,v,t32)	t32=(v)>>32;U32PUT(p,t32);t32=(v);U32PUT((p)+4,t32)

static struct {
	u32int m;
	char *s;
} magics[] = {
	ArenaPartMagic, "ArenaPartMagic",
	ArenaHeadMagic, "ArenaHeadMagic",
	ArenaMagic, "ArenaMagic",
	ISectMagic, "ISectMagic",
	BloomMagic, "BloomMagic"
};

static char*
fmtmagic(char *s, u32int m)
{
	int i;

	for(i=0; i<nelem(magics); i++)
		if(magics[i].m == m)
			return magics[i].s;
	sprint(s, "0x%08ux", m);
	return s;
}

u32int
unpackmagic(u8int *buf)
{
	return U32GET(buf);
}

void
packmagic(u32int magic, u8int *buf)
{
	U32PUT(buf, magic);
}

int
unpackarenapart(ArenaPart *ap, u8int *buf)
{
	u8int *p;
	u32int m;
	char fbuf[20];

	p = buf;

	m = U32GET(p);
	if(m != ArenaPartMagic){
		seterr(ECorrupt, "arena set has wrong magic number: %s expected ArenaPartMagic (%lux)", fmtmagic(fbuf, m), ArenaPartMagic);
		return -1;
	}
	p += U32Size;
	ap->version = U32GET(p);
	p += U32Size;
	ap->blocksize = U32GET(p);
	p += U32Size;
	ap->arenabase = U32GET(p);
	p += U32Size;

	if(buf + ArenaPartSize != p)
		sysfatal("unpackarenapart unpacked wrong amount");

	return 0;
}

int
packarenapart(ArenaPart *ap, u8int *buf)
{
	u8int *p;

	p = buf;

	U32PUT(p, ArenaPartMagic);
	p += U32Size;
	U32PUT(p, ap->version);
	p += U32Size;
	U32PUT(p, ap->blocksize);
	p += U32Size;
	U32PUT(p, ap->arenabase);
	p += U32Size;

	if(buf + ArenaPartSize != p)
		sysfatal("packarenapart packed wrong amount");

	return 0;
}

int
unpackarena(Arena *arena, u8int *buf)
{
	int sz;
	u8int *p;
	u32int m;
	char fbuf[20];

	p = buf;

	m = U32GET(p);
	if(m != ArenaMagic){
		seterr(ECorrupt, "arena has wrong magic number: %s expected ArenaMagic (%lux)", fmtmagic(fbuf, m), ArenaMagic);
		return -1;
	}
	p += U32Size;
	arena->version = U32GET(p);
	p += U32Size;
	namecp(arena->name, (char*)p);
	p += ANameSize;
	arena->diskstats.clumps = U32GET(p);
	p += U32Size;
	arena->diskstats.cclumps = U32GET(p);
	p += U32Size;
	arena->ctime = U32GET(p);
	p += U32Size;
	arena->wtime = U32GET(p);
	p += U32Size;
	if(arena->version == ArenaVersion5){
		arena->clumpmagic = U32GET(p);
		p += U32Size;
	}
	arena->diskstats.used = U64GET(p);
	p += U64Size;
	arena->diskstats.uncsize = U64GET(p);
	p += U64Size;
	arena->diskstats.sealed = U8GET(p);
	p += U8Size;

	arena->memstats = arena->diskstats;

	switch(arena->version){
	case ArenaVersion4:
		sz = ArenaSize4;
		arena->clumpmagic = _ClumpMagic;
		break;
	case ArenaVersion5:
		sz = ArenaSize5;
		break;
	default:
		seterr(ECorrupt, "arena has bad version number %d", arena->version);
		return -1;
	}
	if(buf + sz != p)
		sysfatal("unpackarena unpacked wrong amount");

	return 0;
}

int
packarena(Arena *arena, u8int *buf)
{
	int sz;
	u8int *p;
	u32int t32;

	switch(arena->version){
	case ArenaVersion4:
		sz = ArenaSize4;
		if(arena->clumpmagic != _ClumpMagic)
			fprint(2, "warning: writing old arena tail loses clump magic 0x%lux != 0x%lux\n",
				(ulong)arena->clumpmagic, (ulong)_ClumpMagic);
		break;
	case ArenaVersion5:
		sz = ArenaSize5;
		break;
	default:
		sysfatal("packarena unknown version %d", arena->version);
		return -1;
	}

	p = buf;

	U32PUT(p, ArenaMagic);
	p += U32Size;
	U32PUT(p, arena->version);
	p += U32Size;
	namecp((char*)p, arena->name);
	p += ANameSize;
	U32PUT(p, arena->diskstats.clumps);
	p += U32Size;
	U32PUT(p, arena->diskstats.cclumps);
	p += U32Size;
	U32PUT(p, arena->ctime);
	p += U32Size;
	U32PUT(p, arena->wtime);
	p += U32Size;
	if(arena->version == ArenaVersion5){
		U32PUT(p, arena->clumpmagic);
		p += U32Size;
	}
	U64PUT(p, arena->diskstats.used, t32);
	p += U64Size;
	U64PUT(p, arena->diskstats.uncsize, t32);
	p += U64Size;
	U8PUT(p, arena->diskstats.sealed);
	p += U8Size;

	if(buf + sz != p)
		sysfatal("packarena packed wrong amount");

	return 0;
}

int
unpackarenahead(ArenaHead *head, u8int *buf)
{
	u8int *p;
	u32int m;
	int sz;

	p = buf;

	m = U32GET(p);
	/* XXX check magic! */

	p += U32Size;
	head->version = U32GET(p);
	p += U32Size;
	namecp(head->name, (char*)p);
	p += ANameSize;
	head->blocksize = U32GET(p);
	p += U32Size;
	head->size = U64GET(p);
	p += U64Size;
	if(head->version == ArenaVersion5){
		head->clumpmagic = U32GET(p);
		p += U32Size;
	}

	switch(head->version){
	case ArenaVersion4:
		sz = ArenaHeadSize4;
		head->clumpmagic = _ClumpMagic;
		break;
	case ArenaVersion5:
		sz = ArenaHeadSize5;
		break;
	default:
		seterr(ECorrupt, "arena head has unexpected version %d", head->version);
		return -1;
	}

	if(buf + sz != p)
		sysfatal("unpackarenahead unpacked wrong amount");

	return 0;
}

int
packarenahead(ArenaHead *head, u8int *buf)
{
	u8int *p;
	int sz;
	u32int t32;

	switch(head->version){
	case ArenaVersion4:
		sz = ArenaHeadSize4;
		if(head->clumpmagic != _ClumpMagic)
			fprint(2, "warning: writing old arena header loses clump magic 0x%lux != 0x%lux\n",
				(ulong)head->clumpmagic, (ulong)_ClumpMagic);
		break;
	case ArenaVersion5:
		sz = ArenaHeadSize5;
		break;
	default:
		sysfatal("packarenahead unknown version %d", head->version);
		return -1;
	}

	p = buf;

	U32PUT(p, ArenaHeadMagic);
	p += U32Size;
	U32PUT(p, head->version);
	p += U32Size;
	namecp((char*)p, head->name);
	p += ANameSize;
	U32PUT(p, head->blocksize);
	p += U32Size;
	U64PUT(p, head->size, t32);
	p += U64Size;
	if(head->version == ArenaVersion5){
		U32PUT(p, head->clumpmagic);
		p += U32Size;
	}
	if(buf + sz != p)
		sysfatal("packarenahead packed wrong amount");

	return 0;
}

static int
checkclump(Clump *w)
{
	if(w->encoding == ClumpENone){
		if(w->info.size != w->info.uncsize){
			seterr(ECorrupt, "uncompressed wad size mismatch");
			return -1;
		}
	}else if(w->encoding == ClumpECompress){
		if(w->info.size >= w->info.uncsize){
			seterr(ECorrupt, "compressed lump has inconsistent block sizes %d %d", w->info.size, w->info.uncsize);
			return -1;
		}
	}else{
		seterr(ECorrupt, "clump has illegal encoding");
		return -1;
	}

	return 0;
}

int
unpackclump(Clump *c, u8int *buf, u32int cmagic)
{
	u8int *p;
	u32int magic;

	p = buf;
	magic = U32GET(p);
	if(magic != cmagic){
		seterr(ECorrupt, "clump has bad magic number=%#8.8ux != %#8.8ux", magic, cmagic);
		return -1;
	}
	p += U32Size;

	c->info.type = vtfromdisktype(U8GET(p));
	p += U8Size;
	c->info.size = U16GET(p);
	p += U16Size;
	c->info.uncsize = U16GET(p);
	p += U16Size;
	scorecp(c->info.score, p);
	p += VtScoreSize;

	c->encoding = U8GET(p);
	p += U8Size;
	c->creator = U32GET(p);
	p += U32Size;
	c->time = U32GET(p);
	p += U32Size;

	if(buf + ClumpSize != p)
		sysfatal("unpackclump unpacked wrong amount");

	return checkclump(c);
}

int
packclump(Clump *c, u8int *buf, u32int magic)
{
	u8int *p;

	p = buf;
	U32PUT(p, magic);
	p += U32Size;

	U8PUT(p, vttodisktype(c->info.type));
	p += U8Size;
	U16PUT(p, c->info.size);
	p += U16Size;
	U16PUT(p, c->info.uncsize);
	p += U16Size;
	scorecp(p, c->info.score);
	p += VtScoreSize;

	U8PUT(p, c->encoding);
	p += U8Size;
	U32PUT(p, c->creator);
	p += U32Size;
	U32PUT(p, c->time);
	p += U32Size;

	if(buf + ClumpSize != p)
		sysfatal("packclump packed wrong amount");

	return checkclump(c);
}

void
unpackclumpinfo(ClumpInfo *ci, u8int *buf)
{
	u8int *p;

	p = buf;
	ci->type = vtfromdisktype(U8GET(p));
	p += U8Size;
	ci->size = U16GET(p);
	p += U16Size;
	ci->uncsize = U16GET(p);
	p += U16Size;
	scorecp(ci->score, p);
	p += VtScoreSize;

	if(buf + ClumpInfoSize != p)
		sysfatal("unpackclumpinfo unpacked wrong amount");
}

void
packclumpinfo(ClumpInfo *ci, u8int *buf)
{
	u8int *p;

	p = buf;
	U8PUT(p, vttodisktype(ci->type));
	p += U8Size;
	U16PUT(p, ci->size);
	p += U16Size;
	U16PUT(p, ci->uncsize);
	p += U16Size;
	scorecp(p, ci->score);
	p += VtScoreSize;

	if(buf + ClumpInfoSize != p)
		sysfatal("packclumpinfo packed wrong amount");
}

int
unpackisect(ISect *is, u8int *buf)
{
	u8int *p;
	u32int m;
	char fbuf[20];

	p = buf;


	m = U32GET(p);
	if(m != ISectMagic){
		seterr(ECorrupt, "index section has wrong magic number: %s expected ISectMagic (%lux)",
			fmtmagic(fbuf, m), ISectMagic);
		return -1;
	}
	p += U32Size;
	is->version = U32GET(p);
	p += U32Size;
	namecp(is->name, (char*)p);
	p += ANameSize;
	namecp(is->index, (char*)p);
	p += ANameSize;
	is->blocksize = U32GET(p);
	p += U32Size;
	is->blockbase = U32GET(p);
	p += U32Size;
	is->blocks = U32GET(p);
	p += U32Size;
	is->start = U32GET(p);
	p += U32Size;
	is->stop = U32GET(p);
	p += U32Size;
	if(buf + ISectSize1 != p)
		sysfatal("unpackisect unpacked wrong amount");
	is->bucketmagic = 0;
	if(is->version == ISectVersion2){
		is->bucketmagic = U32GET(p);
		p += U32Size;
		if(buf + ISectSize2 != p)
			sysfatal("unpackisect unpacked wrong amount");
	}

	return 0;
}

int
packisect(ISect *is, u8int *buf)
{
	u8int *p;

	p = buf;

	U32PUT(p, ISectMagic);
	p += U32Size;
	U32PUT(p, is->version);
	p += U32Size;
	namecp((char*)p, is->name);
	p += ANameSize;
	namecp((char*)p, is->index);
	p += ANameSize;
	U32PUT(p, is->blocksize);
	p += U32Size;
	U32PUT(p, is->blockbase);
	p += U32Size;
	U32PUT(p, is->blocks);
	p += U32Size;
	U32PUT(p, is->start);
	p += U32Size;
	U32PUT(p, is->stop);
	p += U32Size;
	if(buf + ISectSize1 != p)
		sysfatal("packisect packed wrong amount");
	if(is->version == ISectVersion2){
		U32PUT(p, is->bucketmagic);
		p += U32Size;
		if(buf + ISectSize2 != p)
			sysfatal("packisect packed wrong amount");
	}

	return 0;
}

void
unpackientry(IEntry *ie, u8int *buf)
{
	u8int *p;

	p = buf;

	scorecp(ie->score, p);
	p += VtScoreSize;
	ie->wtime = U32GET(p);
	p += U32Size;
	ie->train = U16GET(p);
	p += U16Size;
	ie->ia.addr = U64GET(p);
if(ie->ia.addr>>56) print("%.8H => %llux\n", p, ie->ia.addr);
	p += U64Size;
	ie->ia.size = U16GET(p);
	p += U16Size;
	if(p - buf != IEntryTypeOff)
		sysfatal("unpackientry bad IEntryTypeOff amount");
	ie->ia.type = vtfromdisktype(U8GET(p));
	p += U8Size;
	ie->ia.blocks = U8GET(p);
	p += U8Size;

	if(p - buf != IEntrySize)
		sysfatal("unpackientry unpacked wrong amount");
}

void
packientry(IEntry *ie, u8int *buf)
{
	u32int t32;
	u8int *p;

	p = buf;

	scorecp(p, ie->score);
	p += VtScoreSize;
	U32PUT(p, ie->wtime);
	p += U32Size;
	U16PUT(p, ie->train);
	p += U16Size;
	U64PUT(p, ie->ia.addr, t32);
	p += U64Size;
	U16PUT(p, ie->ia.size);
	p += U16Size;
	U8PUT(p, vttodisktype(ie->ia.type));
	p += U8Size;
	U8PUT(p, ie->ia.blocks);
	p += U8Size;

	if(p - buf != IEntrySize)
		sysfatal("packientry packed wrong amount");
}

void
unpackibucket(IBucket *b, u8int *buf, u32int magic)
{
	b->n = U16GET(buf);
	b->data = buf + IBucketSize;
	if(magic && magic != U32GET(buf+U16Size))
		b->n = 0;
}		

void
packibucket(IBucket *b, u8int *buf, u32int magic)
{
	U16PUT(buf, b->n);
	U32PUT(buf+U16Size, magic);
}

void
packbloomhead(Bloom *b, u8int *buf)
{
	u8int *p;

	p = buf;
	U32PUT(p, BloomMagic);
	U32PUT(p+4, BloomVersion);
	U32PUT(p+8, b->nhash);
	U32PUT(p+12, b->size);
}

int
unpackbloomhead(Bloom *b, u8int *buf)
{
	u8int *p;
	u32int m;
	char fbuf[20];

	p = buf;

	m = U32GET(p);
	if(m != BloomMagic){
		seterr(ECorrupt, "bloom filter has wrong magic number: %s expected BloomMagic (%lux)", fmtmagic(fbuf, m), (ulong)BloomMagic);
		return -1;
	}
	p += U32Size;
	
	m = U32GET(p);
	if(m != BloomVersion){
		seterr(ECorrupt, "bloom filter has wrong version %ud expected %ud", (uint)m, (uint)BloomVersion);
		return -1;
	}
	p += U32Size;

	b->nhash = U32GET(p);
	p += U32Size;

	b->size = U32GET(p);
	p += U32Size;

	if(buf + BloomHeadSize != p)
		sysfatal("unpackarena unpacked wrong amount");

	return 0;
}
