/**
 * @file       AccumulatorProofOfKnowledge.h
 *
 * @brief      AccumulatorProofOfKnowledge class for the Zerocoin library.
 *
 * @author     Ian Miers, Christina Garman and Matthew Green
 * @date       June 2013
 *
 * @copyright  Copyright 2013 Ian Miers, Christina Garman and Matthew Green
 * @license    This project is released under the MIT license.
 **/
// Copyright (c) 2017-2019 The PIVX developers

#ifndef ACCUMULATEPROOF_H_
#define ACCUMULATEPROOF_H_

#include "Accumulator.h"
#include "Commitment.h"

namespace libzerocoin {

/**A prove that a value insde the commitment commitmentToCoin is in an accumulator a.
 *
 */
class AccumulatorProofOfKnowledge {
public:
    AccumulatorProofOfKnowledge(){};
	AccumulatorProofOfKnowledge(const AccumulatorAndProofParams* p);

	/** Generates a proof that a commitment to a coin c was accumulated
	 * @param p  Cryptographic parameters
	 * @param commitmentToCoin commitment containing the coin we want to prove is accumulated
	 * @param witness The witness to the accumulation of the coin
	 * @param a
	 */
    AccumulatorProofOfKnowledge(const AccumulatorAndProofParams* p, const Commitment& commitmentToCoin, const AccumulatorWitness& witness);
	/** Verifies that  a commitment c is accumulated in accumulated a
	 */
	bool Verify(const Accumulator& a,const CBigNum& valueOfCommitmentToCoin) const;

	SERIALIZE_METHODS(AccumulatorProofOfKnowledge, obj)
	{
	    READWRITE(obj.C_e);
	    READWRITE(obj.C_u);
	    READWRITE(obj.C_r);
	    READWRITE(obj.st_1);
	    READWRITE(obj.st_2);
	    READWRITE(obj.st_3);
	    READWRITE(obj.t_1);
	    READWRITE(obj.t_2);
	    READWRITE(obj.t_3);
	    READWRITE(obj.t_4);
	    READWRITE(obj.s_alpha);
	    READWRITE(obj.s_beta);
	    READWRITE(obj.s_zeta);
	    READWRITE(obj.s_sigma);
	    READWRITE(obj.s_eta);
	    READWRITE(obj.s_epsilon);
	    READWRITE(obj.s_delta);
	    READWRITE(obj.s_xi);
	    READWRITE(obj.s_phi);
	    READWRITE(obj.s_gamma);
	    READWRITE(obj.s_psi);
  }
private:
	const AccumulatorAndProofParams* params;

	/* Return values for proof */
	CBigNum C_e;
	CBigNum C_u;
	CBigNum C_r;

	CBigNum st_1;
	CBigNum st_2;
	CBigNum st_3;

	CBigNum t_1;
	CBigNum t_2;
	CBigNum t_3;
	CBigNum t_4;

	CBigNum s_alpha;
	CBigNum s_beta;
	CBigNum s_zeta;
	CBigNum s_sigma;
	CBigNum s_eta;
	CBigNum s_epsilon;
	CBigNum s_delta;
	CBigNum s_xi;
	CBigNum s_phi;
	CBigNum s_gamma;
	CBigNum s_psi;
};

} /* namespace libzerocoin */
#endif /* ACCUMULATEPROOF_H_ */
