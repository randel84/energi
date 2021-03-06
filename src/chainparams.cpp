// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2014-2017 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "consensus/merkle.h"

#include "tinyformat.h"
#include "util.h"
#include "utilstrencodings.h"

#include <assert.h>

#include <boost/assign/list_of.hpp>

#include "chainparamsseeds.h"
#include "arith_uint256.h"

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << nBits << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nHeight  = 0;
    genesis.hashMix.SetNull();
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(txNew);
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 *
 * CBlock(hash=00000ffd590b14, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=e0028e, nTime=1390095618, nBits=1e0ffff0, nNonce=28917698, vtx=1)
 *   CTransaction(hash=e0028e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d01044c5957697265642030392f4a616e2f3230313420546865204772616e64204578706572696d656e7420476f6573204c6976653a204f76657273746f636b2e636f6d204973204e6f7720416363657074696e6720426974636f696e73)
 *     CTxOut(nValue=50.00000000, scriptPubKey=0xA9037BAC7050C479B121CF)
 *   vMerkleTree: e0028e
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char* pszTimestamp = "World Power";
    const CScript genesisOutputScript = CScript() << ParseHex("04494295bcacec9dad5aa01f28183f1f27e088cf7e950e21160d2f5eaad024a34eff1112f5cf3bd0fc80754e5cd4a26fde9c6866959e449a5990782c8b60d5f4f5") << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}

bool GenesisCheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return error("GenesisCheckProofOfWork(): nBits below minimum work");

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return error("GenesisCheckProofOfWork(): hash doesn't match nBits");

    return true;
}


//#define ENERGI_MINE_NEW_GENESIS_BLOCK
#ifdef ENERGI_MINE_NEW_GENESIS_BLOCK

#include "dag_singleton.h"
#include "crypto/egihash.h"
#include "validation.h"

#include <chrono>
#include <iomanip>

struct GenesisMiner
{
    GenesisMiner(CBlock & genesisBlock, std::string networkID)
    {
        using namespace std;

        arith_uint256 bnTarget = arith_uint256().SetCompact(genesisBlock.nBits);
        prepare_dag();

        auto start = std::chrono::system_clock::now();

        genesisBlock.nTime = chrono::seconds(time(NULL)).count();
        int i = 0;
        while (true)
        {
            uint256 powHash = genesisBlock.GetPOWHash();

            if ((++i % 250000) == 0)
            {
                auto end = chrono::system_clock::now();
                auto elapsed = chrono::duration_cast<std::chrono::milliseconds>(end - start);
                cout << i << " hashes in " << elapsed.count() / 1000.0 << " seconds ("
                    << static_cast<double>(i) / static_cast<double>(elapsed.count() / 1000.0) << " hps)" << endl;
            }

            if (UintToArith256(powHash) < bnTarget)
            {
                auto end = chrono::system_clock::now();
                auto elapsed = chrono::duration_cast<std::chrono::milliseconds>(end - start);
                cout << "Mined genesis block for " << networkID << " network: 0x" << genesisBlock.GetHash().ToString() << endl
                    << "target was " << bnTarget.ToString() << " POWHash was 0x" << genesisBlock.GetPOWHash().ToString() << endl
                    << "took " << i << " hashes in " << elapsed.count() / 1000.0 << " seconds ("
                    << static_cast<double>(i) / static_cast<double>(elapsed.count() / 1000.0) << " hps)" << endl << endl
                    << genesisBlock.ToString() << endl;
                exit(0);
            }
            genesisBlock.nNonce++;
        }
    }

    void prepare_dag()
    {
        using namespace egihash;
        using namespace std;
        auto const & seedhash = cache_t::get_seedhash(0).to_hex();

        stringstream ss;
        ss << hex << setw(4) << setfill('0') << 0 << "-" << seedhash.substr(0, 12) << ".dag";
        auto const epoch_file = GetDataDir(false) / "dag" / ss.str();

        auto progress = [](::std::size_t step, ::std::size_t max, int phase) -> bool
        {
            switch(phase)
            {
                case cache_seeding:
                    cout << "\rSeeding cache...";
                    break;
                case cache_generation:
                    cout << "\rGenerating cache...";
                    break;
                case cache_saving:
                    cout << "\rSaving cache...";
                    break;
                case cache_loading:
                    cout << "\rLoading cache...";
                    break;
                case dag_generation:
                    cout << "\rGenerating DAG...";
                    break;
                case dag_saving:
                    cout << "\rSaving DAG...";
                    break;
                case dag_loading:
                    cout << "\rLoading DAG...";
                    break;
                default:
                    break;
            }
            cout << fixed << setprecision(2)
            << static_cast<double>(step) / static_cast<double>(max) * 100.0 << "%"
            << setfill(' ') << setw(80) << flush;

            return true;
        };
        unique_ptr<dag_t> new_dag(new dag_t(epoch_file.string(), progress));
        cout << "\r" << endl << endl;
        ActiveDAG(move(new_dag));
    }
};
#endif // ENERGI_MINE_NEW_GENESIS_BLOCK

/**
 * Main network
 */
/**
 * What makes a good checkpoint block?
 * + Is surrounded by blocks with reasonable timestamps
 *   (no blocks before with a timestamp after, none after with
 *    timestamp before)
 * + Contains no strange transactions
 */


class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = "main";

        // Energi distribution parameters
        consensus.energiBackboneScript = CScript() << OP_HASH160 << ParseHex("b051bdceb44b28bb36ef2add5ec07ccbc64708c2") << OP_EQUAL;

        // Seeing as there are 526,000 blocks per year, and there is a 12M annual emission
        // masternodes get 30% of all coins or 3.6M / 526,000 ~ 6.85
        // miners get 20% of all coins or 2.4M / 526,000 ~ 4.57
        // founder gets 10% of all coins or 1.2M / 526,000 ~ 2.28
        // which adds up to 13.7 as block subsidy
        consensus.nBlockSubsidy = 1370000000;
        // 10% to energi backbone
        consensus.nBlockSubsidyBackbone = 228000000;
        // 20% miners
        consensus.nBlockSubsidyMiners = 457000000;
        // 30% masternodes
        // each masternode is paid serially.. more the master nodes more is the wait for the payment
        // masternode payment gap is masternodes minutes
        consensus.nBlockSubsidyMasternodes = 685000000;

        // ensure the sum of the block subsidy parts equals the whole block subsidy
        assert(consensus.nBlockSubsidyBackbone + consensus.nBlockSubsidyMiners + consensus.nBlockSubsidyMasternodes == consensus.nBlockSubsidy);

        // 40% of the total annual emission of ~12M goes to the treasury
        // which is around 4.8M / 26.07 ~ 184,000, where 26.07 are the
        // number of super blocks per year according to the 20160 block cycle
        consensus.nSuperblockCycle = 20160; // (60*24*14) Super block cycle for every 14 days (2 weeks)
        consensus.nRegularTreasuryBudget = 18400000000000;
        consensus.nSpecialTreasuryBudget = 400000000000000; // 4M special initial treasury budget
        consensus.nSpecialTreasuryBudgetBlock = consensus.nSuperblockCycle * 4;

        consensus.nMasternodePaymentsStartBlock = 172800; // should be about 120 days after genesis
        consensus.nInstantSendKeepLock = 24;

        consensus.nBudgetProposalEstablishingTime = 60*60*24; // 1 day

        consensus.nGovernanceMinQuorum = 10;
        consensus.nGovernanceFilterElements = 20000;

        consensus.nMasternodeMinimumConfirmations = 15;
        consensus.nMajorityEnforceBlockUpgrade = 750;
        consensus.nMajorityRejectBlockOutdated = 950;
        consensus.nMajorityWindow = 1000;
        consensus.powLimit = uint256S("00000fffff000000000000000000000000000000000000000000000000000000");

        consensus.nPowTargetTimespan = 24 * 60 * 60; // Dash: 1 day
        consensus.nPowTargetSpacing = 60; // Energi: 1 minute
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1916; // 95% of 2016
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1486252800; // Feb 5th, 2017
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1517788800; // Feb 5th, 2018

        // Deployment of DIP0001
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nStartTime = 1508025600; // Oct 15th, 2017
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nTimeout = 1539561600; // Oct 15th, 2018
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nWindowSize = 4032;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nThreshold = 3226; // 80% of 4032

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x0000000000000000000000000000000000000000000000172210fe351643b3f1"); // 750000

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00000000000000b4181bbbdddbae464ce11fede5d0292fb63fdede1e7c8ab21c"); //750000

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0xec;
        pchMessageStart[1] = 0x2d;
        pchMessageStart[2] = 0x9a;
        pchMessageStart[3] = 0xaf;
        vAlertPubKey = ParseHex("04da7109a0215bf7bb19ecaf9e4295104142b4e03579473c1083ad44e8195a13394a8a7e51ca223fdbc5439420fd08963e491007beab68ac65c5b1c842c8635b37");
        nDefaultPort = 9797;
        nMaxTipAge = 6 * 60 * 60; // ~144 blocks behind -> 2 x fork detection time, was 24 * 60 * 60 in bitcoin
        nDelayGetHeadersTime = 24 * 60 * 60;
        nPruneAfterHeight = 100000;

        genesis = CreateGenesisBlock(1390095618, 28917698, 0x1e0ffff0, 1, consensus.nBlockSubsidyBackbone + consensus.nBlockSubsidyMiners);
        bool const valid_genesis_pow = GenesisCheckProofOfWork(genesis.GetPOWHash(), genesis.nBits, consensus);
        consensus.hashGenesisBlock = genesis.GetHash();

        // TODO: mine genesis block for main net
        //uint256 expectedGenesisHash = uint256S("0x440cbbe939adba25e9e41b976d3daf8fb46b5f6ac0967b0a9ed06a749e7cf1e2");
        //uint256 expectedGenesisMerkleRoot = uint256S("0x6a4855e61ae0da5001564cee6ba8dcd7bc361e9bb12e76b62993390d6db25bca");

        //#ifdef ENERGI_MINE_NEW_GENESIS_BLOCK
        //if (consensus.hashGenesisBlock != expectedGenesisHash)
        //{
        //    GenesisMiner mine(genesis, strNetworkID);
        //}
        //#endif // ENERGI_MINE_NEW_GENESIS_BLOCK
//
        //assert(valid_genesis_pow);
        //assert(consensus.hashGenesisBlock == expectedGenesisHash);
        //assert(genesis.hashMerkleRoot == expectedGenesisMerkleRoot);

        vSeeds.push_back(CDNSSeedData("energi.network", "dnsseed.energi.network"));

        // Energi addresses start with 'E'
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,33);
        // Energi script addresses start with 'N'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,53);
        // Energi private keys start with 'G'
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,106);
        // Energi BIP32 pubkeys start with 'npub'
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x03)(0xB8)(0xC8)(0x56).convert_to_container<std::vector<unsigned char> >();
        // Energi BIP32 prvkeys start with 'nprv'
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0xD7)(0xDC)(0x6E)(0x9F).convert_to_container<std::vector<unsigned char> >();

        // Energi BIP44 coin type is '5'
        nExtCoinType = 5;

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        fMiningRequiresPeers = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;
        fTestnetToBeDeprecatedFieldRPC = false;

        nPoolMaxTransactions = 3;
        nFulfilledRequestExpireTime = 60*60; // fulfilled requests expire in 1 hour
        strSporkPubKey = "044221353eb05b321b55f9b47dc90462066d6e09019e95b05d6603a117877fd34b13b34e8ed005379a9553ce7e719c44c658fd9c9acaae58a04c63cb8f7b5716db";

        checkpointData = (CCheckpointData) {
            boost::assign::map_list_of
            (  1500, uint256S("0x000000aaf0300f59f49bc3e970bad15c11f961fe2347accffff19d96ec9778e3"))
            (  4991, uint256S("0x000000003b01809551952460744d5dbb8fcbd6cbae3c220267bf7fa43f837367"))
            (  9918, uint256S("0x00000000213e229f332c0ffbe34defdaa9e74de87f2d8d1f01af8d121c3c170b"))
            ( 16912, uint256S("0x00000000075c0d10371d55a60634da70f197548dbbfa4123e12abfcbc5738af9"))
            ( 23912, uint256S("0x0000000000335eac6703f3b1732ec8b2f89c3ba3a7889e5767b090556bb9a276"))
            ( 35457, uint256S("0x0000000000b0ae211be59b048df14820475ad0dd53b9ff83b010f71a77342d9f"))
            ( 45479, uint256S("0x000000000063d411655d590590e16960f15ceea4257122ac430c6fbe39fbf02d"))
            ( 55895, uint256S("0x0000000000ae4c53a43639a4ca027282f69da9c67ba951768a20415b6439a2d7"))
            ( 68899, uint256S("0x0000000000194ab4d3d9eeb1f2f792f21bb39ff767cb547fe977640f969d77b7"))
            ( 74619, uint256S("0x000000000011d28f38f05d01650a502cc3f4d0e793fbc26e2a2ca71f07dc3842"))
            ( 75095, uint256S("0x0000000000193d12f6ad352a9996ee58ef8bdc4946818a5fec5ce99c11b87f0d"))
            ( 88805, uint256S("0x00000000001392f1652e9bf45cd8bc79dc60fe935277cd11538565b4a94fa85f"))
            ( 107996, uint256S("0x00000000000a23840ac16115407488267aa3da2b9bc843e301185b7d17e4dc40"))
            ( 137993, uint256S("0x00000000000cf69ce152b1bffdeddc59188d7a80879210d6e5c9503011929c3c"))
            ( 167996, uint256S("0x000000000009486020a80f7f2cc065342b0c2fb59af5e090cd813dba68ab0fed"))
            ( 207992, uint256S("0x00000000000d85c22be098f74576ef00b7aa00c05777e966aff68a270f1e01a5"))
            ( 312645, uint256S("0x0000000000059dcb71ad35a9e40526c44e7aae6c99169a9e7017b7d84b1c2daf"))
            ( 407452, uint256S("0x000000000003c6a87e73623b9d70af7cd908ae22fee466063e4ffc20be1d2dbc"))
            ( 523412, uint256S("0x000000000000e54f036576a10597e0e42cc22a5159ce572f999c33975e121d4d"))
            ( 523930, uint256S("0x0000000000000bccdb11c2b1cfb0ecab452abf267d89b7f46eaf2d54ce6e652c"))
            ( 750000, uint256S("0x00000000000000b4181bbbdddbae464ce11fede5d0292fb63fdede1e7c8ab21c")),
            1507424630, // * UNIX timestamp of last checkpoint block
            3701128,    // * total number of transactions between genesis and last checkpoint
                        //   (the tx=... number in the SetBestChain debug.log lines)
            5000        // * estimated number of transactions per day after checkpoint
        };
    }
};
static CMainParams mainParams;

/**
 * Testnet (v1)
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        strNetworkID = "test";

        // Energi distribution parameters
        consensus.energiBackboneScript = CScript() << OP_DUP << OP_HASH160 << ParseHex("22af89cb590829ae00a927deb2efbf81954c6840") << OP_EQUALVERIFY << OP_CHECKSIG;

        // Seeing as there are 526,000 blocks per year, and there is a 12M annual emission
        // masternodes get 30% of all coins or 3.6M / 526,000 ~ 6.85
        // miners get 20% of all coins or 2.4M / 526,000 ~ 4.57
        // founder gets 10% of all coins or 1.2M / 526,000 ~ 2.28
        // which adds up to 13.7 as block subsidy
        consensus.nBlockSubsidy = 1370000000;
        // 10% founders reward
        consensus.nBlockSubsidyBackbone = 228000000;
        // 20% miners
        // TODO: can remove, since it's not being used anywhere, except to make sure the
        // subsidy parts equal the total block subsidy
        consensus.nBlockSubsidyMiners = 457000000;
        // 30% masternodes
        // each masternode is paid serially.. more the master nodes more is the wait for the payment
        // masternode payment gap is masternodes minutes
        consensus.nBlockSubsidyMasternodes = 685000000;

        // ensure the sum of the block subsidy parts equals the whole block subsidy
        assert(consensus.nBlockSubsidyBackbone + consensus.nBlockSubsidyMiners + consensus.nBlockSubsidyMasternodes == consensus.nBlockSubsidy);

        // 40% of the total annual emission of ~12M goes to the treasury
        // which is around 4.8M / 26.07 ~ 184,000, where 26.07 are the
        // number of super blocks per year according to the 180 block cycle
        consensus.nSuperblockCycle = 30; // 30 Super block cycle for every 1/2 hours
        consensus.nRegularTreasuryBudget = 18400000000000;
        consensus.nSpecialTreasuryBudget = 400000000000000; // 4M special initial treasury budget
        consensus.nSpecialTreasuryBudgetBlock = consensus.nSuperblockCycle * 50;

        consensus.nMasternodePaymentsStartBlock = 172800; // should be about 120 days after genesis
        consensus.nInstantSendKeepLock = 6;
        consensus.nBudgetProposalEstablishingTime = 60*60;
        consensus.nGovernanceMinQuorum = 1;
        consensus.nGovernanceFilterElements = 500;
        consensus.nMasternodeMinimumConfirmations = 1;
        consensus.nMajorityEnforceBlockUpgrade = 51;
        consensus.nMajorityRejectBlockOutdated = 75;
        consensus.nMajorityWindow = 100;
        consensus.powLimit = uint256S("00000fffff000000000000000000000000000000000000000000000000000000");
        consensus.nPowTargetTimespan = 24 * 60 * 60; // in seconds -> Energi: 1 day
        consensus.nPowTargetSpacing = 60; // in seconds Energi: 1 minute
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1512; // 75% for testchains
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1486252800; // Feb 5th, 2017
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1517788800; // Feb 5th, 2018

        pchMessageStart[0] = 0xd9;
        pchMessageStart[1] = 0x2a;
        pchMessageStart[2] = 0xab;
        pchMessageStart[3] = 0x6e;
        vAlertPubKey = ParseHex("04da7109a0215bf7bb19ecaf9e4295104142b4e03579473c1083ad44e8195a13394a8a7e51ca223fdbc5439420fd08963e491007beab68ac65c5b1c842c8635b37");
        nDefaultPort = 19797;
        nMaxTipAge = 0x7fffffff; // allow mining on top of old blocks for testnet
        nDelayGetHeadersTime = 24 * 60 * 60;
        nPruneAfterHeight = 1000;


        genesis = CreateGenesisBlock(1521239519UL, 12501313, 0x1e0ffff0, 1, consensus.nBlockSubsidyBackbone + consensus.nBlockSubsidyMiners);
        bool const valid_genesis_pow = GenesisCheckProofOfWork(genesis.GetPOWHash(), genesis.nBits, consensus);
        consensus.hashGenesisBlock = genesis.GetHash();

        uint256 expectedGenesisHash = uint256S("0x404555947d9a66370f6842c632662b9fae6dd90ef2bf03871af4c4b780ac93f6");
        uint256 expectedGenesisMerkleRoot = uint256S("0x0340ab5702d1b048c256c496df8b4685d3e1b5fbd56c99077de7b91e6220c79a");

        // TODO: mine genesis block for testnet
        #ifdef ENERGI_MINE_NEW_GENESIS_BLOCK
        if (consensus.hashGenesisBlock != expectedGenesisHash)
        {
            GenesisMiner mine(genesis, strNetworkID);
        }
        #endif // ENERGI_MINE_NEW_GENESIS_BLOCK

        assert(valid_genesis_pow);
        assert(consensus.hashGenesisBlock == expectedGenesisHash);
        assert(genesis.hashMerkleRoot == expectedGenesisMerkleRoot);

        vFixedSeeds.clear();
        vSeeds.clear();
        vSeeds.push_back(CDNSSeedData("test.energi.network", "dnsseed.test.energi.network"));

        // Testnet Energi addresses start with 't'
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,127);
        // Testnet Dash script addresses start with '8' or '9'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,19);
        // Testnet private keys start with '9' or 'c' (Bitcoin defaults)
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        // Testnet Dash BIP32 pubkeys start with 'tpub' (Bitcoin defaults)
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x35)(0x87)(0xCF).convert_to_container<std::vector<unsigned char> >();
        // Testnet Dash BIP32 prvkeys start with 'tprv' (Bitcoin defaults)
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x35)(0x83)(0x94).convert_to_container<std::vector<unsigned char> >();

        // Testnet Dash BIP44 coin type is '1' (All coin's testnet default)
        nExtCoinType = 1;

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        fMiningRequiresPeers = false;
        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;
        fTestnetToBeDeprecatedFieldRPC = true;

        nPoolMaxTransactions = 3;
        nFulfilledRequestExpireTime = 5*60; // fulfilled requests expire in 5 minutes
        strSporkPubKey = "044221353eb05b321b55f9b47dc90462066d6e09019e95b05d6603a117877fd34b13b34e8ed005379a9553ce7e719c44c658fd9c9acaae58a04c63cb8f7b5716db";

        checkpointData = (CCheckpointData) {
            boost::assign::map_list_of
            ( 0, uint256S("0x440cbbe939adba25e9e41b976d3daf8fb46b5f6ac0967b0a9ed06a749e7cf1e2")),
			0, // * UNIX timestamp of last checkpoint block
            0,     // * total number of transactions between genesis and last checkpoint
                        //   (the tx=... number in the SetBestChain debug.log lines)
            0         // * estimated number of transactions per day after checkpoint
        };

    }
};
static CTestNetParams testNetParams;


/**
 * Testnet (60x)
 */
class CTestNet60xParams : public CChainParams {
public:
    CTestNet60xParams() {
        strNetworkID = "test60";

        // Energi distribution parameters
        consensus.energiBackboneScript = CScript() << OP_DUP << OP_HASH160 << ParseHex("22af89cb590829ae00a927deb2efbf81954c6840") << OP_EQUALVERIFY << OP_CHECKSIG;
        // Seeing as there are 526,000 blocks per year, and there is a 12M annual emission
        // masternodes get 30% of all coins or 3.6M / 526,000 ~ 6.85
        // miners get 20% of all coins or 2.4M / 526,000 ~ 4.57
        // founder gets 10% of all coins or 1.2M / 526,000 ~ 2.28
        // which adds up to 13.7 as block subsidy
        consensus.nBlockSubsidy = 82200000000; // 1370000000 * 60
        // 10% founders reward
        consensus.nBlockSubsidyBackbone = 13680000000; // 228000000 * 60
        // 20% miners
        consensus.nBlockSubsidyMiners = 27420000000; // 457000000 * 60
        // 30% masternodes
        // each masternode is paid serially.. more the master nodes more is the wait for the payment
        // masternode payment gap is masternodes minutes
        consensus.nBlockSubsidyMasternodes = 41100000000; // 685000000 * 60

        // ensure the sum of the block subsidy parts equals the whole block subsidy
        assert(consensus.nBlockSubsidyBackbone + consensus.nBlockSubsidyMiners + consensus.nBlockSubsidyMasternodes == consensus.nBlockSubsidy);

        // 40% of the total annual emission of ~12M goes to the treasury
        // which is around 4.8M / 26.07 ~ 184,000, where 26.07 are the
        // number of super blocks per year according to the 20160 block cycle
        consensus.nSuperblockCycle = 60;
        consensus.nRegularTreasuryBudget = 18400000000000;
        consensus.nSpecialTreasuryBudget = 400000000000000; // 4M special initial treasury budget
        consensus.nSpecialTreasuryBudgetBlock = consensus.nSuperblockCycle * 36;

        consensus.nMasternodePaymentsStartBlock = 172800 / 60;
        consensus.nInstantSendKeepLock = 6;
        consensus.nBudgetProposalEstablishingTime = 60*20;
        consensus.nGovernanceMinQuorum = 1;
        consensus.nGovernanceFilterElements = 500;
        consensus.nMasternodeMinimumConfirmations = 1;
        consensus.nMajorityEnforceBlockUpgrade = 51;
        consensus.nMajorityRejectBlockOutdated = 75;
        consensus.nMajorityWindow = 100;
        consensus.powLimit = uint256S("00000fffff000000000000000000000000000000000000000000000000000000");
        consensus.nPowTargetTimespan = 24 * 60 * 60; // in seconds -> Energi: 1 day
        consensus.nPowTargetSpacing = 60; // in seconds Energi: 1 minute
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1512; // 75% for testchains
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1486252800; // Feb 5th, 2017
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1517788800; // Feb 5th, 2018

        pchMessageStart[0] = 0xd9;
        pchMessageStart[1] = 0x2a;
        pchMessageStart[2] = 0xab;
        pchMessageStart[3] = 0x60; // Changed the last byte just in case, even though the port is different too, so shouldn't mess with the general testnet
        vAlertPubKey = ParseHex("04da7109a0215bf7bb19ecaf9e4295104142b4e03579473c1083ad44e8195a13394a8a7e51ca223fdbc5439420fd08963e491007beab68ac65c5b1c842c8635b37");
        nDefaultPort = 29797;
        nMaxTipAge = 0x7fffffff; // allow mining on top of old blocks for testnet
        nDelayGetHeadersTime = 24 * 60 * 60;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlock(1521240281UL, 39305453, 0x1e0ffff0, 1, consensus.nBlockSubsidyBackbone + consensus.nBlockSubsidyMiners);
        bool const valid_genesis_pow = GenesisCheckProofOfWork(genesis.GetPOWHash(), genesis.nBits, consensus);
        consensus.hashGenesisBlock = genesis.GetHash();
        uint256 expectedGenesisHash = uint256S("0x5c5a96d7284da7a7c250b6ad2f301893e0f0a61bea4d4a5e009c758e4ea7c39a");
        uint256 expectedGenesisMerkleRoot = uint256S("0x0bb0036bbae578cff5e636a197895f25b7242eeebc0272e91db3ebfb9a73843c");

        // TODO: mine genesis block for testnet60x
        #ifdef ENERGI_MINE_NEW_GENESIS_BLOCK
        if (consensus.hashGenesisBlock != expectedGenesisHash)
        {
            GenesisMiner mine(genesis, strNetworkID);
        }
        #endif // ENERGI_MINE_NEW_GENESIS_BLOCK

        assert(valid_genesis_pow);
        assert(consensus.hashGenesisBlock == expectedGenesisHash);
        assert(genesis.hashMerkleRoot == expectedGenesisMerkleRoot);

        vFixedSeeds.clear();
        vSeeds.clear();
        vSeeds.push_back(CDNSSeedData("test60x.energi.network",  "dnsseed.test60x.energi.network"));

        // Testnet Energi addresses start with 't'
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,127);
        // Testnet Dash script addresses start with '8' or '9'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,19);
        // Testnet private keys start with '9' or 'c' (Bitcoin defaults)
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        // Testnet Dash BIP32 pubkeys start with 'tpub' (Bitcoin defaults)
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x35)(0x87)(0xCF).convert_to_container<std::vector<unsigned char> >();
        // Testnet Dash BIP32 prvkeys start with 'tprv' (Bitcoin defaults)
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x35)(0x83)(0x94).convert_to_container<std::vector<unsigned char> >();

        // Testnet Dash BIP44 coin type is '1' (All coin's testnet default)
        nExtCoinType = 1;

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test60x, pnSeed6_test60x + ARRAYLEN(pnSeed6_test60x));

        fMiningRequiresPeers = false;
        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;
        fTestnetToBeDeprecatedFieldRPC = true;

        nPoolMaxTransactions = 3;
        nFulfilledRequestExpireTime = 5*60; // fulfilled requests expire in 5 minutes
        strSporkPubKey = "044221353eb05b321b55f9b47dc90462066d6e09019e95b05d6603a117877fd34b13b34e8ed005379a9553ce7e719c44c658fd9c9acaae58a04c63cb8f7b5716db";

        checkpointData = (CCheckpointData) {
            boost::assign::map_list_of
            ( 0, uint256S("0x440cbbe939adba25e9e41b976d3daf8fb46b5f6ac0967b0a9ed06a749e7cf1e2")),
            0, // * UNIX timestamp of last checkpoint block
            0,     // * total number of transactions between genesis and last checkpoint
                        //   (the tx=... number in the SetBestChain debug.log lines)
            0         // * estimated number of transactions per day after checkpoint
        };

    }
};
static CTestNet60xParams testNet60xParams;

/**
 * Regression test
 */
class CRegTestParams : public CChainParams {
public:
    CRegTestParams() {
        strNetworkID = "regtest";

        // Energi distribution parameters
        consensus.energiBackboneScript = CScript() << OP_HASH160 << ParseHex("b27ae40d9e9917130210894e50e99f26968faaa4") << OP_EQUAL;

        // Seeing as there are 526,000 blocks per year, and there is a 12M annual emission
        // masternodes get 30% of all coins or 3.6M / 526,000 ~ 6.85
        // miners get 20% of all coins or 2.4M / 526,000 ~ 4.57
        // founder gets 10% of all coins or 1.2M / 526,000 ~ 2.28
        // which adds up to 13.7 as block subsidy
        consensus.nBlockSubsidy = 1370000000;
        // 10% founders reward
        consensus.nBlockSubsidyBackbone = 228000000;
        // 20% miners
        consensus.nBlockSubsidyMiners = 457000000;
        // 30% masternodes
        // each masternode is paid serially.. more the master nodes more is the wait for the payment
        // masternode payment gap is masternodes minutes
        consensus.nBlockSubsidyMasternodes = 685000000;

        // ensure the sum of the block subsidy parts equals the whole block subsidy
        assert(consensus.nBlockSubsidyBackbone + consensus.nBlockSubsidyMiners + consensus.nBlockSubsidyMasternodes == consensus.nBlockSubsidy);

        // 40% of the total annual emission of ~12M goes to the treasury
        // which is around 4.8M / 26.07 ~ 184,000, where 26.07 are the
        // number of super blocks per year according to the 20160 block cycle
        consensus.nSuperblockCycle = 60;
        consensus.nRegularTreasuryBudget = 18400000000000;
        consensus.nSpecialTreasuryBudget = 400000000000000; // 4M special initial treasury budget
        consensus.nSpecialTreasuryBudgetBlock = consensus.nSuperblockCycle * 4;

        consensus.nMasternodePaymentsStartBlock = 240;
        consensus.nInstantSendKeepLock = 6;
        consensus.nBudgetProposalEstablishingTime = 60*20;
        consensus.nGovernanceMinQuorum = 1;
        consensus.nGovernanceFilterElements = 100;
        consensus.nMasternodeMinimumConfirmations = 1;
        consensus.nMajorityEnforceBlockUpgrade = 750;
        consensus.nMajorityRejectBlockOutdated = 950;
        consensus.nMajorityWindow = 1000;
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 24 * 60 * 60; // Dash: 1 day
        consensus.nPowTargetSpacing = 60; // Energi: 1 minute
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = true;
        consensus.nRuleChangeActivationThreshold = 108; // 75% for testchains
        consensus.nMinerConfirmationWindow = 144; // Faster than normal for regtest (144 instead of 2016)
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 999999999999ULL;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 999999999999ULL;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nTimeout = 999999999999ULL;

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00");

        pchMessageStart[0] = 0xef;
        pchMessageStart[1] = 0x89;
        pchMessageStart[2] = 0x6c;
        pchMessageStart[3] = 0x7f;
        nMaxTipAge = 6 * 60 * 60; // ~144 blocks behind -> 2 x fork detection time, was 24 * 60 * 60 in bitcoin
        nDelayGetHeadersTime = 0; // never delay GETHEADERS in regtests
        nDefaultPort = 39797;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlock(1521320241UL, 5, 0x207fffff, 1, consensus.nBlockSubsidyBackbone + consensus.nBlockSubsidyMiners);
        bool const valid_genesis_pow = GenesisCheckProofOfWork(genesis.GetPOWHash(), genesis.nBits, consensus);
        consensus.hashGenesisBlock = genesis.GetHash();

        uint256 expectedGenesisHash = uint256S("0x1fa5bf921cdb7d6daa35ceb88da3cbe9b2ae0dbc0c906b6aa8001879ecbe3ea1");
        uint256 expectedGenesisMerkleRoot = uint256S("0x8afc881131014f95d541ac0f2965eb0397f8d0990ef563e67c540ff767461d15");

        #ifdef ENERGI_MINE_NEW_GENESIS_BLOCK
        if (consensus.hashGenesisBlock != expectedGenesisHash)
        {
            GenesisMiner mine(genesis, strNetworkID);
        }
        #endif // ENERGI_MINE_NEW_GENESIS_BLOCK

        assert(valid_genesis_pow);
        assert(consensus.hashGenesisBlock == expectedGenesisHash);
        assert(genesis.hashMerkleRoot == expectedGenesisMerkleRoot);
        vFixedSeeds.clear(); //! Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();  //! Regtest mode doesn't have any DNS seeds.

        fMiningRequiresPeers = false;
        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;
        fTestnetToBeDeprecatedFieldRPC = false;

        nFulfilledRequestExpireTime = 5*60; // fulfilled requests expire in 5 minutes
        strSporkPubKey = "044221353eb05b321b55f9b47dc90462066d6e09019e95b05d6603a117877fd34b13b34e8ed005379a9553ce7e719c44c658fd9c9acaae58a04c63cb8f7b5716db";

        checkpointData = (CCheckpointData){
            boost::assign::map_list_of
            ( 0, uint256S("0x440cbbe939adba25e9e41b976d3daf8fb46b5f6ac0967b0a9ed06a749e7cf1e2")),
            0,
            0,
            0
        };
        // Testnet Energi addresses start with 't'
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,127);
        // Testnet Dash script addresses start with '8' or '9'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,19);
        // Testnet private keys start with '9' or 'c' (Bitcoin defaults)
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        // Testnet Dash BIP32 pubkeys start with 'tpub' (Bitcoin defaults)
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x35)(0x87)(0xCF).convert_to_container<std::vector<unsigned char> >();
        // Testnet Dash BIP32 prvkeys start with 'tprv' (Bitcoin defaults)
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x35)(0x83)(0x94).convert_to_container<std::vector<unsigned char> >();

        // Regtest Dash BIP44 coin type is '1' (All coin's testnet default)
        nExtCoinType = 1;
   }
};
static CRegTestParams regTestParams;

static CChainParams *pCurrentParams = 0;

const CChainParams &Params() {
    assert(pCurrentParams);
    return *pCurrentParams;
}

CChainParams& Params(const std::string& chain)
{
    if (chain == CBaseChainParams::MAIN)
            return mainParams;
    else if (chain == CBaseChainParams::TESTNET)
            return testNetParams;
        else if (chain == CBaseChainParams::TESTNET60X)
            return testNet60xParams;
    else if (chain == CBaseChainParams::REGTEST)
            return regTestParams;
    else
        throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
    SelectBaseParams(network);
    pCurrentParams = &Params(network);
}
