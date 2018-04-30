﻿// Copyright (c) Henry Cooney 2017

#include "rtik.h"
#include "RangeLimitedFABRIK.h"
#include "Utility/DebugDrawUtil.h"

bool FRangeLimitedFABRIK::SolveRangeLimitedFABRIK(
	const TArray<FTransform>& InTransforms,
	const TArray<FIKBoneConstraint*>& Constraints,
	const FVector & EffectorTargetLocation,
	TArray<FTransform>& OutTransforms,
	float MaxRootDragDistance,
	float RootDragStiffness,
	float Precision,
	int32 MaxIterations,
	ACharacter* Character)
{
	OutTransforms.Empty();

	// Number of points in the chain. Number of bones = NumPoints - 1
	int32 NumPoints = InTransforms.Num();

	// Gather bone transforms
	OutTransforms.Reserve(NumPoints);
	for (const FTransform& Transform : InTransforms)
	{
		OutTransforms.Add(Transform);
	}

	if (NumPoints < 2)
	{
		// Need at least one bone to do IK!
		return false;
	}
	
	// Gather bone lengths. BoneLengths contains the length of the bone ENDING at this point,
	// i.e., BoneLengths[i] contains the distance between point i-1 and point i
	TArray<float> BoneLengths;
	float MaximumReach = ComputeBoneLengths(InTransforms, BoneLengths);

	bool bBoneLocationUpdated = false;
	int32 EffectorIndex       = NumPoints - 1;
	
	// Check distance between tip location and effector location
	float Slop = FVector::Dist(OutTransforms[EffectorIndex].GetLocation(), EffectorTargetLocation);
	if (Slop > Precision)
	{
		// Set tip bone at end effector location.
		OutTransforms[EffectorIndex].SetLocation(EffectorTargetLocation);
		
		int32 IterationCount = 0;
		while ((Slop > Precision) && (IterationCount++ < MaxIterations))
		{
			// "Forward Reaching" stage - adjust bones from end effector.
			FABRIKForwardPass(
				InTransforms,
				Constraints,
				BoneLengths,
				OutTransforms,
				Character
			);	

			// Drag the root if enabled
			DragPointTethered(
				InTransforms[0],
				OutTransforms[1],
				BoneLengths[1],
				MaxRootDragDistance,
				RootDragStiffness,
				OutTransforms[0]
			);

			// "Backward Reaching" stage - adjust bones from root.
			FABRIKBackwardPass(
				InTransforms,
				Constraints,
				BoneLengths,
				OutTransforms,
				Character
			);

			Slop = FMath::Abs(BoneLengths[EffectorIndex] - 
				FVector::Dist(OutTransforms[EffectorIndex - 1].GetLocation(), EffectorTargetLocation));
		}

		// Place effector based on how close we got to the target
		FVector EffectorLocation = OutTransforms[EffectorIndex].GetLocation();
		FVector EffectorParentLocation = OutTransforms[EffectorIndex - 1].GetLocation();
		EffectorLocation = EffectorParentLocation + (EffectorLocation - EffectorParentLocation).GetUnsafeNormal() * BoneLengths[EffectorIndex];
		OutTransforms[EffectorIndex].SetLocation(EffectorLocation);
		
		bBoneLocationUpdated = true;
	}
	
	// Update bone rotations
	if (bBoneLocationUpdated)
	{
		for (int32 PointIndex = 0; PointIndex < NumPoints - 1; ++PointIndex)
		{
			if (!FMath::IsNearlyZero(BoneLengths[PointIndex + 1]))
			{
				UpdateParentRotation(OutTransforms[PointIndex], InTransforms[PointIndex],
					OutTransforms[PointIndex + 1], InTransforms[PointIndex + 1]);
			}
		}
	}

	return bBoneLocationUpdated;
}

bool FRangeLimitedFABRIK::SolveClosedLoopFABRIK(
	const TArray<FTransform>& InTransforms,
	const TArray<FIKBoneConstraint*>& Constraints,
	const FVector& EffectorTargetLocation,
	TArray<FTransform>& OutTransforms,
	float MaxRootDragDistance,
	float RootDragStiffness,
	float Precision,
	int32 MaxIterations,
	ACharacter* Character
)
{
	OutTransforms.Empty();

	// Number of points in the chain. Number of bones = NumPoints - 1
	int32 NumPoints = InTransforms.Num();
	int32 EffectorIndex       = NumPoints - 1;

	// Gather bone transforms
	OutTransforms.Reserve(NumPoints);
	for (const FTransform& Transform : InTransforms)
	{
		OutTransforms.Add(Transform);
	}

	if (NumPoints < 2)
	{
		// Need at least one bone to do IK!
		return false;
	}
	// Gather bone lengths. BoneLengths contains the length of the bone ENDING at this point,
	
	// i.e., BoneLengths[i] contains the distance between point i-1 and point i
	TArray<float> BoneLengths;
	float MaximumReach = ComputeBoneLengths(InTransforms, BoneLengths);
	float RootToEffectorLength = FVector::Dist(InTransforms[0].GetLocation(), InTransforms[EffectorIndex].GetLocation());

	bool bBoneLocationUpdated = false;
	
	// Check distance between tip location and effector location
	float Slop = FVector::Dist(OutTransforms[EffectorIndex].GetLocation(), EffectorTargetLocation);
	if (Slop > Precision)
	{
		// The closed loop method is identical, except the root is dragged a second time to maintain
		// distance with the effector.		

		// Set tip bone at end effector location.
		OutTransforms[EffectorIndex].SetLocation(EffectorTargetLocation);
		
		int32 IterationCount = 0;
		while ((Slop > Precision) && (IterationCount++ < MaxIterations))
		{
			// "Forward Reaching" stage - adjust bones from end effector.
			FABRIKForwardPass(
				InTransforms,
				Constraints,
				BoneLengths,
				OutTransforms,
				Character
			);
			
			// Drag the root if enabled
			DragPointTethered(
				InTransforms[0],
				OutTransforms[1],
				BoneLengths[1],
				MaxRootDragDistance,
				RootDragStiffness,
				OutTransforms[0]
			);

			// Drag the root again, toward the effector (since they're connected in a closed loop)
			DragPointTethered(
				InTransforms[0],
				OutTransforms[EffectorIndex],
				RootToEffectorLength,
				MaxRootDragDistance,
				RootDragStiffness,
				OutTransforms[0]
			);

			// "Backward Reaching" stage - adjust bones from root.
			FABRIKBackwardPass(
				InTransforms,
				Constraints,
				BoneLengths,
				OutTransforms,
				Character
			);

			Slop = FVector::Dist(OutTransforms[EffectorIndex].GetLocation(), EffectorTargetLocation);
		}
				
		bBoneLocationUpdated = true;
	}
	
	// Update bone rotations
	if (bBoneLocationUpdated)
	{
		for (int32 PointIndex = 0; PointIndex < NumPoints - 1; ++PointIndex)
		{
			if (!FMath::IsNearlyZero(BoneLengths[PointIndex + 1]))
			{
				UpdateParentRotation(OutTransforms[PointIndex], InTransforms[PointIndex],
					OutTransforms[PointIndex + 1], InTransforms[PointIndex + 1]);
			}
		}

		// Update the last bone's rotation. Unlike normal fabrik, it's assumed to point toward the root bone,
		// so it's rotation must be updated
		if (!FMath::IsNearlyZero(RootToEffectorLength))
		{
			UpdateParentRotation(OutTransforms[EffectorIndex], InTransforms[EffectorIndex],
				OutTransforms[0], InTransforms[0]);
		}
	}

	return bBoneLocationUpdated;
};

bool FRangeLimitedFABRIK::SolveNoisyThreePoint(
	const FNoisyThreePointClosedLoop& InClosedLoop,
	const FTransform& EffectorAReference,
	const FTransform& EffectorBReference,
	FNoisyThreePointClosedLoop& OutClosedLoop,
	float MaxRootDragDistance,
	float RootDragStiffness,
	float Precision,
	int32 MaxIterations,
	ACharacter* Character
)
{
	// Temporary transforms for each point
	FTransform A    = InClosedLoop.EffectorATransform;
	FTransform B    = InClosedLoop.EffectorBTransform;
	FTransform Root = InClosedLoop.RootTransform;

	// Compute bone lengths
	float DistAToRoot = InClosedLoop.TargetRootADistance;
	float DistBToRoot = InClosedLoop.TargetRootBDistance;
	float DistAToB    = InClosedLoop.TargetABDistance;
	float DistARef    = FVector::Dist(A.GetLocation(), EffectorAReference.GetLocation());
	float DistBRef    = FVector::Dist(B.GetLocation(), EffectorBReference.GetLocation());

	// Now start the noisy solver method. The idea here is that A, B, and Root are out of whack;
	// move them so inter-joint distances are satisfied again. Keep doing this until things settle down.

	// See www.andreasaristidou.com/publications/papers/Extending_FABRIK_with_Model_Cοnstraints.pdf Figure 9 for
	// description of each phase. Hopefully I'm implementing this right; unfortunatley the paper is vague

	FVector LastA = A.GetLocation();
	FVector LastB = B.GetLocation();

	int32 IterationCount = 0;

	// Phase 1 (Fig. 9 b): go around the loop
	DragPoint(Root, DistAToRoot, A);
	DragPoint(A, DistAToB, B);
	DragPointTethered(InClosedLoop.RootTransform, B, DistBToRoot, MaxRootDragDistance, RootDragStiffness, Root);
	DragPoint(Root, DistAToRoot, A);

	// Phase 2 (Fig. 9 c): Reset root and go other way
	Root.SetLocation(InClosedLoop.RootTransform.GetLocation());
	DragPoint(Root, DistBToRoot, B);
	DragPoint(B, DistAToB, A);

	// Phase 3 (Fig. 9 d): Drag both effectors such that their distances to reference points (outside the closed loop)
	// and distances from root are maintained
	DragPoint(Root, DistAToRoot, A);
	DragPoint(EffectorAReference, DistARef, A);	
	DragPoint(Root, DistBToRoot, B);
	DragPoint(EffectorBReference, DistBRef, B);	

	// Phase 4 (Fig. 9 b): Same as phase 1
	DragPoint(Root, DistAToRoot, A);
	DragPoint(A, DistAToB, B);
	DragPointTethered(InClosedLoop.RootTransform, B, DistBToRoot, MaxRootDragDistance, RootDragStiffness, Root);
	DragPoint(Root, DistAToRoot, A);

	// Phase 5 (Fig. 9 c): Same as phase 2, but don't reset root
	DragPoint(Root, DistBToRoot, B);
	DragPoint(B, DistAToB, A);

	float PrecisionSq = Precision * Precision;
	float Delta = FMath::Max(FVector::DistSquared(A.GetLocation(), LastA), FVector::Dist(B.GetLocation(), LastB));
	LastA = A.GetLocation();
	LastB = B.GetLocation();

	while ((Delta > PrecisionSq) && (IterationCount++ < MaxIterations))
	{
		// Iterate phases 3-5 only
		// Phase 3
		DragPoint(Root, DistAToRoot, A);
		DragPoint(EffectorAReference, DistARef, A);
		DragPoint(Root, DistBToRoot, B);
		DragPoint(EffectorBReference, DistBRef, B);
		
		// Phase 4
		DragPoint(Root, DistAToRoot, A);
		DragPoint(A, DistAToB, B);
		DragPointTethered(InClosedLoop.RootTransform, B, DistBToRoot, MaxRootDragDistance, RootDragStiffness, Root);
		DragPoint(Root, DistAToRoot, A);
				
		// Phase 5
		DragPoint(Root, DistBToRoot, B);
		DragPoint(B, DistAToB, A);

		Delta = FMath::Max(FVector::DistSquared(A.GetLocation(), LastA), FVector::Dist(B.GetLocation(), LastB));
		LastA = A.GetLocation();
		LastB = B.GetLocation();		
	}
	
	// Update rotations
	if (!FMath::IsNearlyZero(DistAToRoot))
	{
		UpdateParentRotation(Root, InClosedLoop.RootTransform, A, InClosedLoop.EffectorATransform);
	}
	
	if (!FMath::IsNearlyZero(DistAToB))
	{
		UpdateParentRotation(A, InClosedLoop.EffectorATransform, B, InClosedLoop.EffectorBTransform);
	}

	if (!FMath::IsNearlyZero(DistBToRoot))
	{
		UpdateParentRotation(B, InClosedLoop.EffectorBTransform, Root, InClosedLoop.RootTransform);
	}
	
	// Copy transforms to output
	OutClosedLoop.EffectorATransform = A;
	OutClosedLoop.EffectorBTransform = B;
	OutClosedLoop.RootTransform = Root;

	return true;
}

void FRangeLimitedFABRIK::FABRIKForwardPass(
	const TArray<FTransform>& InTransforms,
	const TArray<FIKBoneConstraint*>& Constraints,
	const TArray<float>& BoneLengths,
	TArray<FTransform>& OutTransforms,
	ACharacter* Character
)
{
	int32 NumPoints     = InTransforms.Num();
	int32 EffectorIndex = NumPoints - 1;

	for (int32 PointIndex = EffectorIndex - 1; PointIndex > 0; --PointIndex)
	{
		FTransform& CurrentPoint = OutTransforms[PointIndex];
		FTransform& ChildPoint = OutTransforms[PointIndex + 1];

		// Move the parent to maintain starting bone lengths
		DragPoint(ChildPoint, BoneLengths[PointIndex + 1], CurrentPoint);

		// Enforce parent's constraint any time child is moved
		FIKBoneConstraint* CurrentConstraint = Constraints[PointIndex - 1];
		if (CurrentConstraint != nullptr && CurrentConstraint->bEnabled)
		{
			CurrentConstraint->SetupFn(
				PointIndex - 1,
				InTransforms,
				Constraints,
				OutTransforms
			);

			CurrentConstraint->EnforceConstraint(
				PointIndex - 1,
				InTransforms,
				Constraints,
				OutTransforms,
				Character
			);
		}
	}
}
	
void FRangeLimitedFABRIK::FABRIKBackwardPass(
	const TArray<FTransform>& InTransforms,
	const TArray<FIKBoneConstraint*>& Constraints,
	const TArray<float>& BoneLengths,
	TArray<FTransform>& OutTransforms,
	ACharacter* Character
	)
{
	int32 NumPoints     = InTransforms.Num();
	int32 EffectorIndex = NumPoints - 1;

	for (int32 PointIndex = 1; PointIndex < EffectorIndex; PointIndex++)
	{
		FTransform& ParentPoint  = OutTransforms[PointIndex - 1];
		FTransform& CurrentPoint = OutTransforms[PointIndex];
		
		// Move the child to maintain starting bone lengths
		DragPoint(ParentPoint, BoneLengths[PointIndex], CurrentPoint);
		
		// Enforce parent's constraint any time child is moved
		FIKBoneConstraint* CurrentConstraint = Constraints[PointIndex - 1];
		if (CurrentConstraint != nullptr && CurrentConstraint->bEnabled)
		{
			CurrentConstraint->SetupFn(
				PointIndex - 1,
				InTransforms,
				Constraints,
				OutTransforms
			);
			
			CurrentConstraint->EnforceConstraint(
				PointIndex - 1,
				InTransforms,
				Constraints,
				OutTransforms,
				Character
			);
		}
	}
}

FORCEINLINE void FRangeLimitedFABRIK::DragPoint(
	const FTransform& MaintainDistancePoint,
	float BoneLength,
	FTransform& PointToMove
) 
{
	PointToMove.SetLocation(MaintainDistancePoint.GetLocation() +
		(PointToMove.GetLocation() - MaintainDistancePoint.GetLocation()).GetUnsafeNormal() *
		BoneLength);
}

void FRangeLimitedFABRIK::DragPointTethered(
	const FTransform& StartingTransform,
	const FTransform& MaintainDistancePoint,
	float BoneLength,
	float MaxDragDistance,
	float DragStiffness,
	FTransform& PointToDrag
)
{
	if (MaxDragDistance < KINDA_SMALL_NUMBER || DragStiffness < KINDA_SMALL_NUMBER)
	{
		PointToDrag = StartingTransform;
		return;
	}  
		
	FVector Target;
	if (FMath::IsNearlyZero(BoneLength))
	{
		Target = MaintainDistancePoint.GetLocation();
	}
	else
	{
		Target = MaintainDistancePoint.GetLocation() +
			(PointToDrag.GetLocation() - MaintainDistancePoint.GetLocation()).GetUnsafeNormal() *
			BoneLength;
	}
		
	FVector Displacement = Target - StartingTransform.GetLocation();

	// Root drag stiffness 'pulls' the root back (set to 1.0 to disable)
	Displacement /= DragStiffness;	
	
	// limit root displacement to drag length
	FVector LimitedDisplacement = Displacement.GetClampedToMaxSize(MaxDragDistance);
	PointToDrag.SetLocation(StartingTransform.GetLocation() + LimitedDisplacement);
}

void FRangeLimitedFABRIK::UpdateParentRotation(
	FTransform& NewParentTransform, 
	const FTransform& OldParentTransform,
	const FTransform& NewChildTransform,
	const FTransform& OldChildTransform)
{
	FVector OldDir = (OldChildTransform.GetLocation() - OldParentTransform.GetLocation()).GetUnsafeNormal();
	FVector NewDir = (NewChildTransform.GetLocation() - NewParentTransform.GetLocation()).GetUnsafeNormal();
	
	FVector RotationAxis = FVector::CrossProduct(OldDir, NewDir).GetSafeNormal();
	float RotationAngle  = FMath::Acos(FVector::DotProduct(OldDir, NewDir));
	FQuat DeltaRotation  = FQuat(RotationAxis, RotationAngle);
	
	NewParentTransform.SetRotation(DeltaRotation * OldParentTransform.GetRotation());
	NewParentTransform.NormalizeRotation();
}

float FRangeLimitedFABRIK::ComputeBoneLengths(
	const TArray<FTransform>& InTransforms,
	TArray<float>& OutBoneLengths
)
{
	int32 NumPoints = InTransforms.Num();
	float MaximumReach = 0.0f;
	OutBoneLengths.Empty();
	OutBoneLengths.Reserve(NumPoints);

	// Root always has zero length
	OutBoneLengths.Add(0.0f);

	for (int32 i = 1; i < NumPoints; ++i)
	{
		OutBoneLengths.Add(FVector::Dist(InTransforms[i - 1].GetLocation(),
			InTransforms[i].GetLocation()));
		MaximumReach  += OutBoneLengths[i];
	}
	
	return MaximumReach;
}
