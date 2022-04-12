// (C) Copyright 2018-2022 Simul Software Ltd

#pragma once

#include "Parser.h"
#include "BitReader.h"
#include "HevcTypes.h"

#include <map>
#include <list>
#include <memory>

namespace avparser
{ 
#define IS_HEVC_IDR(n) ((n)->header.type == hevc::NALUnitType::IDR_W_RADL || (n)->header.type == hevc::NALUnitType::IDR_N_LP)
#define IS_HEVC_BLA(n) ((n)->header.type == hevc::NALUnitType::BLA_W_RADL || (n)->header.type == hevc::NALUnitType::BLA_W_LP || (n)->header.type == hevc::NALUnitType::BLA_N_LP)
#define IS_HEVC_IRAP(n) ((uint32_t)(n)->header.type >= 16 && (uint32_t)(n)->header.type <= 23)

    namespace hevc
    {
        // Useful data extracted from the raw data of the last slice parsed.
        struct ExtraData
        {
            uint32_t poc = 0;
            uint32_t stRpsIdx = 0;
            uint32_t refRpsIdx = 0;
            uint32_t short_term_ref_pic_set_size = 0;
            uint32_t numDeltaPocsOfRefRpsIdx = 0;
            std::vector<uint32_t> longTermRefPicPocs;
        };

        class HevcParser : public Parser
        {
        public:
            HevcParser(bool shortSliceParse = false);

            size_t parseNALUnit(const uint8_t* data, size_t size) override;

            void* getVPS() const override
            {
                return (void*)&mVPS;
            }

            void* getSPS() const override
            {
                return (void*)&mSPS;
            }

            void* getPPS()  const override
            {
                return (void*)&mPPS;
            }

            void* getLastSlice() const override
            {
                return (void*)&mLastSlice;
            }

            void* getExtraData() const override
            {
                return (void*)&mExtraData;
            }

        private:
            void parseNALUnitHeader(NALHeader& header);

            void parseVPS(VPS& vps);

            void parseSPS(SPS& sps);
            ProfileTierLevel parseProfileTierLevel(size_t max_sub_layers_minus1);

            void parsePPS(PPS& pps);
            HrdParameters parseHrdParameters(uint8_t commonInfPresentFlag, size_t maxNumSubLayersMinus1);
            SubLayerHrdParameters parseSubLayerHrdParameters(uint8_t sub_pic_hrd_params_present_flag, size_t cpb_cnt_minus1);
            ShortTermRefPicSet parseShortTermRefPicSet(uint32_t stRpsIdx, uint32_t num_short_term_ref_pic_sets, const std::vector<ShortTermRefPicSet>& refPicSets, SPS& sps, uint32_t& refRpsIdx);
            VuiParameters parseVuiParameters(size_t sps_max_sub_layers_minus1);
            ScalingListData parseScalingListData();

            void parseSliceHeader(Slice& slice);
            RefPicListModification parseRefPicListModification(Slice& slice);
            PredWeightTable parsePredWeightTable(Slice& slice);

            static uint32_t log2(uint32_t k);
            static uint32_t computeNumPocTotal(Slice& slice, SPS& sps);

            static uint32_t computePoc(const SPS& sps, uint32_t prevPocTid0, uint32_t pocLsb, uint32_t nalUnitType);


            static constexpr uint8_t mLog2Table[256] = {
                0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
                5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
                6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
                6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
                7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
                7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
                7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
                7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
            };

            VPS mVPS;
            SPS mSPS;
            PPS mPPS;
            Slice mLastSlice;
            // This will be reset before parsing a new slice.
            ExtraData mExtraData;

            std::unique_ptr<BitReader> mReader;

            uint32_t mPrevPocTid0;

            // True if slice parsing should exclude the modification list and weight table. 
            bool mShortSliceParse;
        };
    }
}

