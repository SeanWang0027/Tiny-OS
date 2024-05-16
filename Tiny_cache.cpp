#include "Tiny_cache.h"
#include "Tiny_buf.h"
extern Disk myDisk;

BufferManager::BufferManager()
{
    Initialize();
}

BufferManager::~BufferManager()
{
	Bflush();
	delete bFreeList;
}

void BufferManager::Format() 
{
	for (int i = 0; i < NBUF; i++)
		m_Buf[i].Reset();
	Initialize();
}


void BufferManager::Initialize()
{
	for (int i = 0; i < NBUF; i++) {
		if (i) 
			m_Buf[i].b_forw = m_Buf + i - 1;
		else {
			m_Buf[i].b_forw = bFreeList;
			bFreeList->b_back = m_Buf + i;
		}

		if (i + 1 < NBUF)
			m_Buf[i].b_back = m_Buf + i + 1;
		else {
			m_Buf[i].b_back = bFreeList;
			bFreeList->b_forw = m_Buf + i;
		}
		m_Buf[i].b_addr = Buffer[i];
		m_Buf[i].b_no = i;
	}
}

Buf* BufferManager::GetBlk(int blkno) 
{
	Buf* bp;
	if (map.find(blkno) != map.end()) {
		bp = map[blkno];
		detach(bp);
		return bp;
	}
	bp = bFreeList->b_back;
	if (bp == bFreeList) {
		cout << "Currently we do not have free Buffer!" << endl;
		return NULL;
	}
	detach(bp);
	map.erase(bp->b_blkno);
	if (bp->b_flags & Buf::B_DELWRI)
		myDisk.write(bp->b_addr, BUFFER_SIZE, bp->b_blkno * BUFFER_SIZE);
	bp->b_flags &= ~(Buf::B_DELWRI | Buf::B_DONE);
	bp->b_blkno = blkno;
	map[blkno] = bp;
	return bp;
}

void BufferManager::Brelse(Buf* bp)
{
	insert(bp);
}



Buf* BufferManager::Bread(int blkno)
{
	Buf* bp;
	bp = this->GetBlk(blkno);
	if(bp->b_flags & (Buf::B_DONE | Buf::B_DELWRI)){
		return bp;
	}
	myDisk.read(bp->b_addr, BUFFER_SIZE, blkno * BUFFER_SIZE);
	bp->b_flags |= (Buf::B_DONE);
	bp->b_wcount = BufferManager::BUFFER_SIZE;
	return bp;
}

void BufferManager::Bwrite(Buf *bp)
{
	bp->b_flags &= ~( Buf::B_DELWRI);
	myDisk.write(bp->b_addr, BUFFER_SIZE, bp->b_blkno * BUFFER_SIZE);
	bp->b_flags |= (Buf::B_DONE);
	this->Brelse(bp);
}

void BufferManager::Bdwrite(Buf *bp)
{
	bp->b_flags |= (Buf::B_DELWRI | Buf::B_DONE);
	this->Brelse(bp);
	return;
}


void BufferManager::Bclear(Buf *bp)
{
	int* pInt = (int *)bp->b_addr;

	for(unsigned int i = 0; i < BufferManager::BUFFER_SIZE / sizeof(int); i++)	{
		pInt[i] = 0;
	}
	return;
}

void BufferManager::Bflush()
{
	Buf* bp = NULL;
	for (int i = 0; i < NBUF; ++i) {
		bp = m_Buf + i;
		if ((bp->b_flags & Buf::B_DELWRI)) {
			bp->b_flags &= ~(Buf::B_DELWRI);
			myDisk.write(bp->b_addr, BUFFER_SIZE, bp->b_blkno * BUFFER_SIZE);
			bp->b_flags |= (Buf::B_DONE);
		}
	}
}

void BufferManager::detach(Buf* bp) 
{
	if (bp->b_back == NULL)
		return;
	bp->b_forw->b_back = bp->b_back;
	bp->b_back->b_forw = bp->b_forw;
	bp->b_back = NULL;
	bp->b_forw = NULL;
}

void BufferManager::insert(Buf* bp) 
{
	if (bp->b_back != NULL)
		return;
	bp->b_forw = bFreeList->b_forw;
	bp->b_back = bFreeList;
	bFreeList->b_forw->b_back = bp;
	bFreeList->b_forw = bp;
}