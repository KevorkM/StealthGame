// Fill out your copyright notice in the Description page of Project Settings.


#include "FPSAIGard.h"
#include "GameFramework/Actor.h"
#include "Runtime/Engine/Classes/Engine/World.h"
#include "Perception/PawnSensingComponent.h"
#include "FPSGameMode.h"
#include "DrawDebugHelpers.h"
#include "Components/PawnNoiseEmitterComponent.h"
#include "Runtime/Engine/Classes/Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"

// Sets default values
AFPSAIGard::AFPSAIGard()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	PawnSensingComp = CreateDefaultSubobject<UPawnSensingComponent>(TEXT("PawnSensingComp"));
	PawnSensingComp->OnHearNoise.AddDynamic(this, &AFPSAIGard::OnNoiseHeard);

	SetGuardState(EAIState::Idle);
}

// Called when the game starts or when spawned
void AFPSAIGard::BeginPlay()
{
	Super::BeginPlay();
	PawnSensingComp->OnSeePawn.AddDynamic(this, &AFPSAIGard::OnPawnSeen);

	OriginalRotation = GetActorRotation();

	if (bPatrol)
	{
		MoveToNextPatrolPoint();
	}

}

void AFPSAIGard::OnPawnSeen(APawn * SeenPawn)
{
	if (SeenPawn == nullptr) { return; }
	DrawDebugSphere(GetWorld(), SeenPawn->GetActorLocation(), 32.f, 12, FColor::Purple, false, 10.f);

	AFPSGameMode* GM = Cast<AFPSGameMode>(GetWorld()->GetAuthGameMode());

	if (GM) {

		GM->CompleteMission(SeenPawn, false);
	}
	SetGuardState(EAIState::Alerted);

	// Stop Movement if Patrolling
	AController* Controller = GetController();
	if (Controller)
	{
		Controller->StopMovement();
	}
}

void AFPSAIGard::OnNoiseHeard(APawn * NoiseInstigator, const FVector & Location, float Volume){

	//no longer interested in hearing noises
	if (GuardState == EAIState::Alerted) { return; }

	DrawDebugSphere(GetWorld(), Location, 32.f, 12, FColor::Red, false, 10.f);

	//make the pawn rotate when hearing a noise
	FVector Direction = Location - GetActorLocation();
	Direction.Normalize();
	FRotator NewLookAt = FRotationMatrix::MakeFromX(Direction).Rotator();

	// This code makes the pawn stand straight
	NewLookAt.Pitch = 0.f;
	NewLookAt.Roll = 0.f;

	SetActorRotation(NewLookAt);

	//setting a timer

	GetWorldTimerManager().ClearTimer(TimerHandle_ResetOrientation);
	GetWorldTimerManager().SetTimer(TimerHandle_ResetOrientation, this, &AFPSAIGard::ResetOrientation, 3.0f);

		SetGuardState(EAIState::Suspicious);
	
		// Stop Movement if Patrolling
		AController* Controller = GetController();
		if (Controller)
		{
			Controller->StopMovement();
		}

}


void AFPSAIGard::ResetOrientation()
{
	if (GuardState != EAIState::Alerted) { return; }
	SetActorRotation(OriginalRotation);

	SetGuardState(EAIState::Idle);

	// Stopped investigating...if we are a patrolling pawn, pick a new patrol point to move to
	if (bPatrol)
	{
		MoveToNextPatrolPoint();
	}
}
void AFPSAIGard::OnRep_GuardState()
{
	OnStateChanged(GuardState);
}

void AFPSAIGard::SetGuardState(EAIState NewState)
{
	if (GuardState == NewState)
	{
		return;
	}

	GuardState = NewState;
	OnRep_GuardState();
}


// Called every frame
void AFPSAIGard::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// Patrol Goal Checks
	if (CurrentPatrolPoint)
	{
		FVector Delta = GetActorLocation() - CurrentPatrolPoint->GetActorLocation();
		float DistanceToGoal = Delta.Size();

		// Check if we are within 50 units of our goal, if so - pick a new patrol point
		if (DistanceToGoal < 50)
		{
			MoveToNextPatrolPoint();
		}
	}
}
	void AFPSAIGard::MoveToNextPatrolPoint()
	{
		// Assign next patrol point.
		if (CurrentPatrolPoint == nullptr || CurrentPatrolPoint == SecondPatrolPoint)
		{
			CurrentPatrolPoint = FirstPatrolPoint;
		}
		else
		{
			CurrentPatrolPoint = SecondPatrolPoint;
		}

		UAIBlueprintHelperLibrary::SimpleMoveToActor(GetController(), CurrentPatrolPoint);
	}


	void AFPSAIGard::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
	{
		Super::GetLifetimeReplicatedProps(OutLifetimeProps);

		DOREPLIFETIME(AFPSAIGard, GuardState);
	}
