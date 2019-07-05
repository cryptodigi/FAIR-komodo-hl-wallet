
/******************************************************************************
 * Copyright © 2014-2019 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

// todo:
// make sure no files are updated (this is to allow nSPV=1 and later nSPV=0 without affecting database)
// validate proofs
// make sure to sanity check all vector lengths on receipt
// determine if it makes sense to be scanning mempool for the utxo/spentinfo requests

#ifndef KOMODO_NSPV_H
#define KOMODO_NSPV_H

// nSPV defines and struct definitions with serialization and purge functions

#define NSPV_INFO 0x00
#define NSPV_INFORESP 0x01
#define NSPV_UTXOS 0x02
#define NSPV_UTXOSRESP 0x03
#define NSPV_NTZS 0x04
#define NSPV_NTZSRESP 0x05
#define NSPV_NTZSPROOF 0x06
#define NSPV_NTZSPROOFRESP 0x07
#define NSPV_TXPROOF 0x08
#define NSPV_TXPROOFRESP 0x09
#define NSPV_SPENTINFO 0x0a
#define NSPV_SPENTINFORESP 0x0b

int32_t iguana_rwbuf(int32_t rwflag,uint8_t *serialized,uint16_t len,uint8_t *buf)
{
    if ( rwflag != 0 )
        memcpy(serialized,buf,len);
    else memcpy(buf,serialized,len);
    return(len);
}

struct NSPV_equihdr
{
    int32_t nVersion;
    uint256 hashPrevBlock;
    uint256 hashMerkleRoot;
    uint256 hashFinalSaplingRoot;
    uint32_t nTime;
    uint32_t nBits;
    uint256 nNonce;
    uint8_t nSolution[1344];
};

int32_t NSPV_rwequihdr(int32_t rwflag,uint8_t *serialized,struct NSPV_equihdr *ptr)
{
    int32_t len = 0;
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->nVersion),&ptr->nVersion);
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->hashPrevBlock),(uint8_t *)&ptr->hashPrevBlock);
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->hashMerkleRoot),(uint8_t *)&ptr->hashMerkleRoot);
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->hashFinalSaplingRoot),(uint8_t *)&ptr->hashFinalSaplingRoot);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->nTime),&ptr->nTime);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->nBits),&ptr->nBits);
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->nNonce),(uint8_t *)&ptr->nNonce);
    len += iguana_rwbuf(rwflag,&serialized[len],sizeof(ptr->nSolution),ptr->nSolution);
    return(len);
}

int32_t iguana_rwequihdrvec(int32_t rwflag,uint8_t *serialized,uint16_t *vecsizep,struct NSPV_equihdr **ptrp)
{
    int32_t i,vsize,len = 0;
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(*vecsizep),vecsizep);
    if ( (vsize= *vecsizep) != 0 )
    {
        //fprintf(stderr,"vsize.%d ptrp.%p alloc %ld\n",vsize,*ptrp,sizeof(struct NSPV_equihdr)*vsize);
        if ( *ptrp == 0 )
            *ptrp = (struct NSPV_equihdr *)calloc(sizeof(struct NSPV_equihdr),vsize); // relies on uint16_t being "small" to prevent mem exhaustion
        for (i=0; i<vsize; i++)
            len += NSPV_rwequihdr(rwflag,&serialized[len],&(*ptrp)[i]);
    }
    return(len);
}

int32_t iguana_rwuint8vec(int32_t rwflag,uint8_t *serialized,uint16_t *vecsizep,uint8_t **ptrp)
{
    int32_t vsize,len = 0;
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(*vecsizep),vecsizep);
    if ( (vsize= *vecsizep) != 0 )
    {
        if ( *ptrp == 0 )
            *ptrp = (uint8_t *)calloc(1,vsize); // relies on uint16_t being "small" to prevent mem exhaustion
        len += iguana_rwbuf(rwflag,&serialized[len],vsize,*ptrp);
    }
    return(len);
}

struct NSPV_utxoresp
{
    uint256 txid;
    int64_t satoshis,extradata;
    int32_t vout,height;
};

int32_t NSPV_rwutxoresp(int32_t rwflag,uint8_t *serialized,struct NSPV_utxoresp *ptr)
{
    int32_t len = 0;
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->txid),(uint8_t *)&ptr->txid);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->satoshis),&ptr->satoshis);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->extradata),&ptr->extradata);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->vout),&ptr->vout);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->height),&ptr->height);
    return(len);
}

struct NSPV_utxosresp
{
    struct NSPV_utxoresp *utxos;
    char coinaddr[64];
    int64_t total,interest;
    int32_t nodeheight;
    uint16_t numutxos,pad16;
};

int32_t NSPV_rwutxosresp(int32_t rwflag,uint8_t *serialized,struct NSPV_utxosresp *ptr) // check mempool
{
    int32_t i,len = 0;
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->numutxos),&ptr->numutxos);
    if ( ptr->numutxos != 0 )
    {
        if ( ptr->utxos == 0 )
            ptr->utxos = (struct NSPV_utxoresp *)calloc(sizeof(*ptr->utxos),ptr->numutxos); // relies on uint16_t being "small" to prevent mem exhaustion
        for (i=0; i<ptr->numutxos; i++)
            len += NSPV_rwutxoresp(rwflag,&serialized[len],&ptr->utxos[i]);
    }
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->total),&ptr->total);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->interest),&ptr->interest);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->nodeheight),&ptr->nodeheight);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->pad16),&ptr->pad16);
    if ( rwflag != 0 )
    {
        memcpy(&serialized[len],ptr->coinaddr,sizeof(ptr->coinaddr));
        len += sizeof(ptr->coinaddr);
    }
    else
    {
        memcpy(ptr->coinaddr,&serialized[len],sizeof(ptr->coinaddr));
        len += sizeof(ptr->coinaddr);
    }
    return(len);
}

void NSPV_utxosresp_purge(struct NSPV_utxosresp *ptr)
{
    if ( ptr != 0 )
    {
        if ( ptr->utxos != 0 )
            free(ptr->utxos);
        memset(ptr,0,sizeof(*ptr));
    }
}

struct NSPV_ntz
{
    uint256 blockhash,txid,othertxid;
    int32_t height,txidheight;
};

int32_t NSPV_rwntz(int32_t rwflag,uint8_t *serialized,struct NSPV_ntz *ptr)
{
    int32_t len = 0;
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->blockhash),(uint8_t *)&ptr->blockhash);
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->txid),(uint8_t *)&ptr->txid);
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->othertxid),(uint8_t *)&ptr->othertxid);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->height),&ptr->height);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->txidheight),&ptr->txidheight);
    return(len);
}

struct NSPV_ntzsresp
{
    struct NSPV_ntz prevntz,nextntz;
};

int32_t NSPV_rwntzsresp(int32_t rwflag,uint8_t *serialized,struct NSPV_ntzsresp *ptr)
{
    int32_t len = 0;
    len += NSPV_rwntz(rwflag,&serialized[len],&ptr->prevntz);
    len += NSPV_rwntz(rwflag,&serialized[len],&ptr->nextntz);
    return(len);
}

void NSPV_ntzsresp_purge(struct NSPV_ntzsresp *ptr)
{
    if ( ptr != 0 )
        memset(ptr,0,sizeof(*ptr));
}

struct NSPV_inforesp
{
    struct NSPV_ntz notarization;
    uint256 blockhash;
    int32_t height,pad32;
};

int32_t NSPV_rwinforesp(int32_t rwflag,uint8_t *serialized,struct NSPV_inforesp *ptr)
{
    int32_t len = 0;
    len += NSPV_rwntz(rwflag,&serialized[len],&ptr->notarization);
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->blockhash),(uint8_t *)&ptr->blockhash);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->height),&ptr->height);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->pad32),&ptr->pad32);
    return(len);
}

void NSPV_inforesp_purge(struct NSPV_inforesp *ptr)
{
    if ( ptr != 0 )
        memset(ptr,0,sizeof(*ptr));
}

struct NSPV_txproof
{
    uint256 txid;
    int32_t height;
    uint16_t txlen,txprooflen;
    uint8_t *tx,*txproof;
};

int32_t NSPV_rwtxproof(int32_t rwflag,uint8_t *serialized,struct NSPV_txproof *ptr)
{
    int32_t len = 0;
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->txid),(uint8_t *)&ptr->txid);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->height),&ptr->height);
    len += iguana_rwuint8vec(rwflag,&serialized[len],&ptr->txlen,&ptr->tx);
    len += iguana_rwuint8vec(rwflag,&serialized[len],&ptr->txprooflen,&ptr->txproof);
    return(len);
}

void NSPV_txproof_purge(struct NSPV_txproof *ptr)
{
    if ( ptr != 0 )
    {
        if ( ptr->tx != 0 )
            free(ptr->tx);
        if ( ptr->txproof != 0 )
            free(ptr->txproof);
        memset(ptr,0,sizeof(*ptr));
    }
}

struct NSPV_utxo
{
    struct NSPV_txproof T;
    int64_t satoshis,extradata;
    int32_t vout,prevht,nextht,pad32;
};

int32_t NSPV_rwutxo(int32_t rwflag,uint8_t *serialized,struct NSPV_utxo *ptr)
{
    int32_t len = 0;
    len += NSPV_rwtxproof(rwflag,&serialized[len],&ptr->T);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->satoshis),&ptr->satoshis);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->extradata),&ptr->extradata);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->vout),&ptr->vout);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->prevht),&ptr->prevht);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->nextht),&ptr->nextht);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->pad32),&ptr->pad32);
    return(len);
}

struct NSPV_ntzproofshared
{
    struct NSPV_equihdr *hdrs;
    int32_t prevht,nextht,pad32;
    uint16_t numhdrs,pad16;
};

int32_t NSPV_rwntzproofshared(int32_t rwflag,uint8_t *serialized,struct NSPV_ntzproofshared *ptr)
{
    int32_t len = 0;
    len += iguana_rwequihdrvec(rwflag,&serialized[len],&ptr->numhdrs,&ptr->hdrs);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->prevht),&ptr->prevht);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->nextht),&ptr->nextht);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->pad32),&ptr->pad32);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->pad16),&ptr->pad16);
    return(len);
}

struct NSPV_ntzsproofresp
{
    struct NSPV_ntzproofshared common;
    uint256 prevtxid,nexttxid;
    int32_t pad32,prevtxidht,nexttxidht;
    uint16_t prevtxlen,nexttxlen;
    uint8_t *prevntz,*nextntz;
};

int32_t NSPV_rwntzsproofresp(int32_t rwflag,uint8_t *serialized,struct NSPV_ntzsproofresp *ptr)
{
    int32_t len = 0;
    len += NSPV_rwntzproofshared(rwflag,&serialized[len],&ptr->common);
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->prevtxid),(uint8_t *)&ptr->prevtxid);
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->nexttxid),(uint8_t *)&ptr->nexttxid);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->pad32),&ptr->pad32);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->prevtxidht),&ptr->prevtxidht);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->nexttxidht),&ptr->nexttxidht);
    len += iguana_rwuint8vec(rwflag,&serialized[len],&ptr->prevtxlen,&ptr->prevntz);
    len += iguana_rwuint8vec(rwflag,&serialized[len],&ptr->nexttxlen,&ptr->nextntz);
    return(len);
}

void NSPV_ntzsproofresp_purge(struct NSPV_ntzsproofresp *ptr)
{
    if ( ptr != 0 )
    {
        if ( ptr->common.hdrs != 0 )
            free(ptr->common.hdrs);
        if ( ptr->prevntz != 0 )
            free(ptr->prevntz);
        if ( ptr->nextntz != 0 )
            free(ptr->nextntz);
        memset(ptr,0,sizeof(*ptr));
    }
}

struct NSPV_MMRproof
{
    struct NSPV_ntzproofshared common;
    // tbd
};

struct NSPV_spentinfo
{
    struct NSPV_txproof spent;
    uint256 txid;
    int32_t vout,spentvini;
};

int32_t NSPV_rwspentinfo(int32_t rwflag,uint8_t *serialized,struct NSPV_spentinfo *ptr) // check mempool
{
    int32_t len = 0;
    len += NSPV_rwtxproof(rwflag,&serialized[len],&ptr->spent);
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->txid),(uint8_t *)&ptr->txid);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->vout),&ptr->vout);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->spentvini),&ptr->spentvini);
    return(len);
}

void NSPV_spentinfo_purge(struct NSPV_spentinfo *ptr)
{
    if ( ptr != 0 )
    {
        NSPV_txproof_purge(&ptr->spent);
        memset(ptr,0,sizeof(*ptr));
    }
}

#endif // KOMODO_NSPV_H