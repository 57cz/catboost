#pragma once

#include "pairwise_score_calcer_for_policy.h"
#include <catboost/cuda/gpu_data/compressed_index.h>
#include <catboost/libs/options/oblivious_tree_options.h>
#include <catboost/cuda/methods/helpers.h>
#include <catboost/cuda/methods/pairwise_kernels.h>

namespace NCatboostCuda {
    inline THolder<TComputePairwiseScoresHelper> CreateScoreHelper(EFeaturesGroupingPolicy policy,
                                                                   const TCompressedDataSet<TDocParallelLayout>& dataSet,
                                                                   const TPairwiseOptimizationSubsets& subsets,
                                                                   const NCatboostOptions::TObliviousTreeLearnerOptions& treeConfig) {
        return new TComputePairwiseScoresHelper(policy,
                                                dataSet,
                                                subsets,
                                                treeConfig.MaxDepth,
                                                treeConfig.L2Reg,
                                                treeConfig.PairwiseNonDiagReg);
    };

    struct TBestSplitResult {
        TBestSplitProperties BestSplit;
        TAtomicSharedPtr<TVector<float>> Solution;
        bool operator<(const TBestSplitResult& other) const {
            return BestSplit.operator<(other.BestSplit);
        }
    };

    class TPairwiseScoreCalcer {
    public:
        using TLayoutPolicy = TDocParallelLayout;
        using TSamplesMapping = typename TLayoutPolicy::TSamplesMapping;

        TPairwiseScoreCalcer(const TCompressedDataSet<TLayoutPolicy>& features,
                             const NCatboostOptions::TObliviousTreeLearnerOptions& treeConfig,
                             const TPairwiseOptimizationSubsets& subsets,
                             bool storeSolverTempResults = false)
            : Features(features)
            , Subsets(subsets)
            , TreeConfig(treeConfig)
            , StoreTempResults(storeSolverTempResults)
        {
            for (auto policy : GetAllGroupingPolicies()) {
                if (Features.GetGridSize(policy)) {
                    Helpers[policy] = CreateScoreHelper(policy,
                                                        Features,
                                                        Subsets,
                                                        TreeConfig);
                }
            }
        }

        bool HasHelperForPolicy(EFeaturesGroupingPolicy policy) const {
            return Helpers.has(policy);
        }

        const TComputePairwiseScoresHelper& GetHelperForPolicy(EFeaturesGroupingPolicy policy) const {
            CB_ENSURE(Helpers.has(policy));
            return *Helpers.at(policy);
        }

        const TBinaryFeatureSplitResults& GetResultsForPolicy(EFeaturesGroupingPolicy policy) const {
            CB_ENSURE(Helpers.has(policy));
            return *Solutions.at(policy);
        }

        void Compute();

        TBestSplitResult FindOptimalSplit(bool needBestSolution);

    private:
        const TCompressedDataSet<TLayoutPolicy>& Features;
        const TPairwiseOptimizationSubsets& Subsets;
        const NCatboostOptions::TObliviousTreeLearnerOptions& TreeConfig;
        bool StoreTempResults;

        using TScoreHelperPtr = THolder<TComputePairwiseScoresHelper>;
        using TResultPtr = THolder<TBinaryFeatureSplitResults>;

        TMap<EFeaturesGroupingPolicy, TScoreHelperPtr> Helpers;
        TMap<EFeaturesGroupingPolicy, TResultPtr> Solutions;
    };

}
