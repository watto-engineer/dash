#ifndef ZWGR_WITNESS_H
#define ZWGR_WITNESS_H


#include <libzerocoin/Accumulator.h>
#include <libzerocoin/Coin.h>
#include "zerocoin.h"
#include "serialize.h"

#define PRECOMPUTE_LRU_CACHE_SIZE 1000
#define PRECOMPUTE_MAX_DIRTY_CACHE_SIZE 100
#define PRECOMPUTE_FLUSH_TIME 300 // 5 minutes

class CoinWitnessCacheData;

class CoinWitnessData
{
public:
    std::unique_ptr<libzerocoin::PublicCoin> coin;
    std::unique_ptr<libzerocoin::Accumulator> pAccumulator;
    std::unique_ptr<libzerocoin::AccumulatorWitness> pWitness;
    libzerocoin::CoinDenomination denom;
    int nHeightCheckpoint;
    int nHeightMintAdded;
    int nHeightAccStart;
    int nHeightAccEnd;
    int nMintsAdded;
    uint256 txid;
    bool isV1;

    CoinWitnessData();
    CoinWitnessData(CZerocoinMint& mint);
    CoinWitnessData(CoinWitnessCacheData& data);
    void SetHeightMintAdded(int nHeight);
    void SetNull();
    std::string ToString();
};

class CoinWitnessCacheData
{
public:
    libzerocoin::CoinDenomination denom;
    int nHeightCheckpoint;
    int nHeightMintAdded;
    int nHeightAccStart;
    int nHeightAccEnd;
    int nMintsAdded;
    uint256 txid;
    bool isV1;
    CBigNum coinAmount;
    libzerocoin::CoinDenomination coinDenom;
    CBigNum accumulatorAmount;
    libzerocoin::CoinDenomination accumulatorDenom;

    CoinWitnessCacheData();
    CoinWitnessCacheData(CoinWitnessData* coinWitnessData);
    void SetNull();

    SERIALIZE_METHODS(CoinWitnessData, obj)
    {
        READWRITE(obj.denom);
        READWRITE(obj.nHeightCheckpoint);
        READWRITE(obj.nHeightMintAdded);
        READWRITE(obj.nHeightAccStart);
        READWRITE(obj.nHeightAccEnd);
        READWRITE(obj.nMintsAdded);
        READWRITE(obj.txid);
        READWRITE(obj.isV1);
        READWRITE(obj.coinAmount); // used to create the PublicCoin
        READWRITE(obj.coinDenom);
        READWRITE(obj.accumulatorAmount); // used to create the pAccumulator
        READWRITE(obj.accumulatorDenom);
    };
};
#endif //ZWGR_WITNESS_H
