// Copyright (c) Henry Cooney 2017

#pragma once

#include "AnimGraphNode_SkeletalControlBase.h"
#include "IK/AnimNode_HumanoidLegIKKneeCorrection.h"
#include "AnimGraphNode_HumanoidLegIKKneeCorrection.generated.h"

UCLASS()
class RTIKEDITOR_API UAnimGraphNode_HumanoidLegIKKneeCorrection : public UAnimGraphNode_SkeletalControlBase
{
	GENERATED_BODY()
	
public:

	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FLinearColor GetNodeTitleColor() const override;
	FString GetNodeCategory() const override;

protected:
	virtual FText GetControllerDescription() const;
protected:
	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimNode_HumanoidLegIKKneeCorrection Node;

};