
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

#ifndef KOMODO_NSPVWALLET_H
#define KOMODO_NSPVWALLET_H

// nSPV wallet uses superlite functions (and some komodod built in functions) to implement nSPV_send

int32_t NSPV_gettransaction(uint256 txid,int32_t height,CTransaction &tx)
{
    char *txstr; int32_t retval = 0;
    NSPV_txproof(txid,height);
    if ( NSPV_txproofresult.txid != txid || NSPV_txproofresult.height != height )
        return(-1);
    txstr = (char *)malloc(NSPV_txproofresult.txlen*2 + 1);
    init_hexbytes_noT(txstr,NSPV_txproofresult.tx,NSPV_txproofresult.txlen);
    if ( !DecodeHexTx(tx,txstr) )
        retval = -1;
    else
    {
        // need to validate txproof
        // get the notarizations bracket of height
        // get hdrsproof of the prev,next
        // verify the hdrs link back from next to prev
        // merkle prove txproof to the merkleroot in the corresponding hdr
    }
    free(txstr);
    return(retval);
}

int32_t NSPV_vinselect(int32_t *aboveip,int64_t *abovep,int32_t *belowip,int64_t *belowp,struct NSPV_utxoresp utxos[],int32_t numunspents,int64_t value)
{
    int32_t i,abovei,belowi; int64_t above,below,gap,atx_value;
    abovei = belowi = -1;
    for (above=below=i=0; i<numunspents; i++)
    {
        if ( (atx_value= utxos[i].satoshis) <= 0 )
            continue;
        if ( atx_value == value )
        {
            *aboveip = *belowip = i;
            *abovep = *belowp = 0;
            return(i);
        }
        else if ( atx_value > value )
        {
            gap = (atx_value - value);
            if ( above == 0 || gap < above )
            {
                above = gap;
                abovei = i;
            }
        }
        else
        {
            gap = (value - atx_value);
            if ( below == 0 || gap < below )
            {
                below = gap;
                belowi = i;
            }
        }
        //printf("value %.8f gap %.8f abovei.%d %.8f belowi.%d %.8f\n",dstr(value),dstr(gap),abovei,dstr(above),belowi,dstr(below));
    }
    *aboveip = abovei;
    *abovep = above;
    *belowip = belowi;
    *belowp = below;
    //printf("above.%d below.%d\n",abovei,belowi);
    if ( abovei >= 0 && belowi >= 0 )
    {
        if ( above < (below >> 1) )
            return(abovei);
        else return(belowi);
    }
    else if ( abovei >= 0 )
        return(abovei);
    else return(belowi);
}

int64_t NSPV_addinputs(struct NSPV_utxoresp *used,CMutableTransaction &mtx,int64_t total,int32_t maxinputs,struct NSPV_utxoresp *ptr,int32_t num)
{
    int32_t abovei,belowi,ind,vout,i,n = 0; int64_t threshold,above,below; int64_t remains,totalinputs = 0; CTransaction tx; struct NSPV_utxoresp utxos[NSPV_MAXVINS],*up;
    memset(utxos,0,sizeof(utxos));
    if ( maxinputs > NSPV_MAXVINS )
        maxinputs = NSPV_MAXVINS;
    if ( maxinputs > 0 )
        threshold = total/maxinputs;
    else threshold = total;
    for (i=0; i<num; i++)
    {
        if ( ptr[i].satoshis > threshold )
            utxos[n++] = ptr[i];
    }
    remains = total;
    //fprintf(stderr,"n.%d for total %.8f\n",n,(double)total/COIN);
    for (i=0; i<maxinputs && n>0; i++)
    {
        below = above = 0;
        abovei = belowi = -1;
        if ( NSPV_vinselect(&abovei,&above,&belowi,&below,utxos,n,remains) < 0 )
        {
            fprintf(stderr,"error finding unspent i.%d of %d, %.8f vs %.8f\n",i,n,(double)remains/COIN,(double)total/COIN);
            return(0);
        }
        if ( belowi < 0 || abovei >= 0 )
            ind = abovei;
        else ind = belowi;
        if ( ind < 0 )
        {
            fprintf(stderr,"error finding unspent i.%d of %d, %.8f vs %.8f, abovei.%d belowi.%d ind.%d\n",i,n,(double)remains/COIN,(double)total/COIN,abovei,belowi,ind);
            return(0);
        }
        //fprintf(stderr,"i.%d ind.%d abovei.%d belowi.%d n.%d\n",i,ind,abovei,belowi,n);
        up = &utxos[ind];
        mtx.vin.push_back(CTxIn(up->txid,up->vout,CScript()));
        used[i] = *up;
        totalinputs += up->satoshis;
        remains -= up->satoshis;
        utxos[ind] = utxos[--n];
        memset(&utxos[n],0,sizeof(utxos[n]));
        fprintf(stderr,"totalinputs %.8f vs total %.8f i.%d vs max.%d\n",(double)totalinputs/COIN,(double)total/COIN,i,maxinputs);
        if ( totalinputs >= total || (i+1) >= maxinputs )
            break;
    }
    fprintf(stderr,"totalinputs %.8f vs total %.8f\n",(double)totalinputs/COIN,(double)total/COIN);
    if ( totalinputs >= total )
        return(totalinputs);
    return(0);
}

bool NSPV_SignTx(CMutableTransaction &mtx,int32_t vini,int64_t utxovalue,const CScript scriptPubKey)
{
    CTransaction txNewConst(mtx); SignatureData sigdata; CBasicKeyStore keystore;
    auto consensusBranchId = CurrentEpochBranchId(chainActive.Height() + 1, Params().GetConsensus());
    keystore.AddKey(NSPV_key);
    if ( ProduceSignature(TransactionSignatureCreator(&keystore,&txNewConst,vini,utxovalue,SIGHASH_ALL),scriptPubKey,sigdata,consensusBranchId) != 0 )
    {
        UpdateTransaction(mtx,vini,sigdata);
        return(true);
    } else fprintf(stderr,"signing error for SignTx vini.%d %.8f\n",vini,(double)utxovalue/COIN);
    return(false);
}

std::string NSPV_signtx(CMutableTransaction &mtx,uint64_t txfee,CScript opret,struct NSPV_utxoresp used[])
{
    CTransaction vintx; std::string hex; uint256 hashBlock; int64_t change,totaloutputs=0,totalinputs=0; int32_t i,utxovout,n;
    n = mtx.vout.size();
    for (i=0; i<n; i++)
        totaloutputs += mtx.vout[i].nValue;
    PrecomputedTransactionData txdata(mtx);
    for (i=0; i<n; i++)
    {
        if ( NSPV_gettransaction(mtx.vin[i].prevout.hash,used[i].height,vintx) == 0 )
        {
            utxovout = mtx.vin[i].prevout.n;
            totalinputs += vintx.vout[utxovout].nValue;
            if ( vintx.vout[utxovout].nValue != used[i].satoshis )
            {
                fprintf(stderr,"vintx mismatch %.8f != %.8f\n",(double)vintx.vout[utxovout].nValue/COIN,(double)used[i].satoshis/COIN);
                return("");
            }
            else if ( utxovout != used[i].vout )
            {
                fprintf(stderr,"vintx vout mismatch %d != %d\n",utxovout,used[i].vout);
                return("");
            }
            else if ( SignTx(mtx,i,vintx.vout[utxovout].nValue,vintx.vout[utxovout].scriptPubKey) == 0 )
                fprintf(stderr,"signing error for vini.%d\n",i);
        } else fprintf(stderr,"couldnt find txid.%s\n",mtx.vin[i].prevout.hash.GetHex().c_str());
    }
    if ( totalinputs >= totaloutputs+2*txfee )
    {
        change = totalinputs - (totaloutputs+txfee);
        mtx.vout.push_back(CTxOut(change,CScript() << ParseHex(NSPV_pubkeystr) << OP_CHECKSIG));
    }
    if ( opret.size() > 0 )
        mtx.vout.push_back(CTxOut(0,opret));
    fprintf(stderr,"sign %d inputs %.8f -> %d outputs %.8f change %.8f\n",(int32_t)mtx.vin.size(),(double)totalinputs/COIN,(int32_t)mtx.vout.size(),(double)totaloutputs/COIN,(double)change/COIN);
    return(EncodeHexTx(mtx));
}

UniValue NSPV_send(char *srcaddr,char *destaddr,int64_t satoshis) // what its all about!
{
    UniValue result(UniValue::VOBJ); uint8_t rmd160[128]; int64_t txfee = 10000;
    if ( strcmp(srcaddr,NSPV_address.c_str()) != 0 )
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","invalid address"));
        result.push_back(Pair("mismatched",srcaddr));
        return(result);
    }
    else if ( bitcoin_base58decode(rmd160,destaddr) != 25 )
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","invalid destaddr"));
        return(result);
    }
    if ( NSPV_inforesult.height == 0 )
        NSPV_getinfo_json();
    if ( NSPV_inforesult.height == 0 )
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","couldnt getinfo"));
        return(result);
    }
    if ( strcmp(NSPV_utxosresult.coinaddr,srcaddr) != 0 || NSPV_utxosresult.nodeheight < NSPV_inforesult.height )
        NSPV_addressutxos(srcaddr);
    if ( strcmp(NSPV_utxosresult.coinaddr,srcaddr) != 0 || NSPV_utxosresult.nodeheight < NSPV_inforesult.height )
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","couldnt get addressutxos"));
        return(result);
    }
    if ( NSPV_utxosresult.total < satoshis+txfee )
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","not enough funds"));
        result.push_back(Pair("balance",(double)NSPV_utxosresult.total/COIN));
        result.push_back(Pair("amount",(double)satoshis/COIN));
        return(result);
    }
    printf("%s numutxos.%d balance %.8f\n",NSPV_utxosresult.coinaddr,NSPV_utxosresult.numutxos,(double)NSPV_utxosresult.total/COIN);
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    std::vector<uint8_t> data; CScript opret; std::string hex;
    struct NSPV_utxoresp used[NSPV_MAXVINS];
    memset(used,0,sizeof(used));
    data.resize(20);
    memcpy(&data[0],&rmd160[1],20);
    if ( NSPV_addinputs(used,mtx,satoshis+txfee,64,NSPV_utxosresult.utxos,NSPV_utxosresult.numutxos) > 0 )
    {
        mtx.vout.push_back(CTxOut(satoshis,CScript() << OP_DUP << OP_HASH160 << ParseHex(HexStr(data)) << OP_EQUALVERIFY << OP_CHECKSIG));
        hex = NSPV_signtx(mtx,txfee,opret,used);
        result.push_back(Pair("result","success"));
        result.push_back(Pair("hex",hex));
        // prove all the vins
        return(result);
    }
    else
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","couldnt create tx"));
        return(result);
    }
}

// polling loop (really this belongs in its own file, but it is so small, it ended up here)

void komodo_nSPV(CNode *pto) // polling loop from SendMessages
{
    uint8_t msg[256]; int32_t i,len=0; uint32_t timestamp = (uint32_t)time(NULL);
    if ( NSPV_logintime != 0 && timestamp > NSPV_logintime+60 )
    {
        fprintf(stderr,"scrub wif from NSPV memory\n");
        memset(NSPV_wifstr,0,sizeof(NSPV_wifstr));
        memset(&NSPV_key,0,sizeof(NSPV_key));
        NSPV_logintime = 0;
    }
    if ( (pto->nServices & NODE_NSPV) == 0 )
        return;
    if ( KOMODO_NSPV != 0 )
    {
        if ( timestamp > NSPV_lastinfo + ASSETCHAINS_BLOCKTIME/2 && timestamp > pto->prevtimes[NSPV_INFO>>1] + 2*ASSETCHAINS_BLOCKTIME/3 )
        {
            len = 0;
            msg[len++] = NSPV_INFO;
            NSPV_req(pto,msg,len,NODE_NSPV,NSPV_INFO>>1);
        }
    }
}

#endif // KOMODO_NSPVWALLET_H