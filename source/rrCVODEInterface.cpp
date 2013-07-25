#ifdef USE_PCH
#include "rr_pch.h"
#endif
#pragma hdrstop
#include "rrCVODEInterface.h"
#include "rrExecutableModel.h"
#include "rrException.h"
#include "rrLogger.h"
#include "rrStringUtils.h"
#include "rrException.h"
#include "rrUtils.h"
#include "rrEvent.h"

#include <cvode/cvode.h>
#include <cvode/cvode_dense.h>
#include <nvector/nvector_serial.h>
#include <cstring>
#include <iomanip>
#include <math.h>
#include <map>
#include <algorithm>

#include <Poco/Logger.h>


using namespace std;
namespace rr
{

int InternalFunctionCall(realtype t, N_Vector cv_y, N_Vector cv_ydot, void *f_data);
int InternalRootCall (realtype t, N_Vector y, realtype *gout, void *g_data);

void ModelFcn(int n,     double time, double* y, double* ydot, void* userData);
void EventFcn(            double time, double* y, double* gdot, void* userData);

// N_Vector is a point to an N_Vector structure
RR_DECLSPEC void        SetVector (N_Vector v, int Index, double Value);
RR_DECLSPEC double      GetVector (N_Vector v, int Index);

const int CvodeInterface::mDefaultMaxNumSteps = 10000;
const int CvodeInterface::mDefaultMaxAdamsOrder = 12;
const int CvodeInterface::mDefaultMaxBDFOrder = 5;

CvodeInterface::CvodeInterface(ExecutableModel *aModel, const double _absTol,
        const double _relTol)
:
mDefaultReltol(_relTol),
mDefaultAbsTol(_absTol),
mStateVector(NULL),
mAbstolArray(NULL),
mCVODE_Memory(NULL),
mLastTimeValue(0),
mLastEvent(0),
mOneStepCount(0),
mFollowEvents(true),
mMaxAdamsOrder(mDefaultMaxAdamsOrder),
mMaxBDFOrder(mDefaultMaxBDFOrder),
mInitStep(0.0),
mMinStep(0.0),
mMaxStep(0.0),
mMaxNumSteps(mDefaultMaxNumSteps),
mRelTol(_relTol),
mAbsTol(_absTol),
paramBDFOrder("BDFOrder", mMaxBDFOrder, "Maximum order for BDF Method"),
paramAdamsOrder("AdamsOrder", mMaxAdamsOrder, "Maximum order for Adams Method"),
paramRTol("rtol", mRelTol, "Relative Tolerance"),
paramATol("atol", mAbsTol, "Absolute Tolerance"),
paramMaxSteps("maxsteps", mMaxNumSteps, "Maximum number of internal stepsc"),
paramInitSteps("initstep", mInitStep, "the initial step size"),
paramMinStep("minstep", mMinStep, "specifies a lower bound on the magnitude of the step size."),
paramMaxStep("maxstep", mMaxStep, "specifies an upper bound on the    magnitude of the step size."),
mCVODECapability("Integration", "CVODE", "CVODE Integrator")
{
    //Setup capability
    mCVODECapability.addParameter(&paramBDFOrder);
    mCVODECapability.addParameter(&paramAdamsOrder);
    mCVODECapability.addParameter(&paramRTol);
    mCVODECapability.addParameter(&paramATol);
    mCVODECapability.addParameter(&paramMaxSteps);
    mCVODECapability.addParameter(&paramInitSteps);
    mCVODECapability.addParameter(&paramMinStep);
    mCVODECapability.addParameter(&paramMaxStep);

    if(aModel)
    {
        initializeCVODEInterface(aModel);
    }
}

CvodeInterface::~CvodeInterface()
{
    //CVode crashes if handed NULL vectorc... (: ! ........
    if(mCVODE_Memory)
    {
        CVodeFree( &mCVODE_Memory);
    }

    if(mStateVector)
    {
        N_VDestroy_Serial(mStateVector);
    }

    if(mAbstolArray)
    {
        N_VDestroy_Serial(mAbstolArray);
    }
}

void CvodeInterface::setTolerances(const double& aTol, const double& rTol)
{
    mAbsTol = aTol;
    mRelTol = rTol;
}

ExecutableModel* CvodeInterface::getModel()
{
    return mModel;
}

Capability& CvodeInterface::getCapability()
{
    return mCVODECapability;
}

int CvodeInterface::allocateCvodeMem ()
{
    if (mCVODE_Memory == NULL)
    {
        return CV_SUCCESS;
    }

    double t0 = 0.0;
    if(CVodeSetUserData(mCVODE_Memory, (void*) this) != CV_SUCCESS)
    {
        Log(lError)<<"Problem in setting CVODE User data";
    }

    int result =  CVodeInit(mCVODE_Memory, InternalFunctionCall, t0, mStateVector);

    if (result != CV_SUCCESS)
    {
        return result;
    }
    return CVodeSVtolerances(mCVODE_Memory, mRelTol, mAbstolArray);
}

int CvodeInterface::rootInit (const int& numRoots)//, TRootCallBack callBack, void *gdata)
{
    if (mCVODE_Memory == NULL)
    {
         return CV_SUCCESS;
    }

    return CVodeRootInit (mCVODE_Memory, numRoots, InternalRootCall);
}

// Initialize cvode with a new set of initial conditions
//int CvodeInterface::CVReInit (void *cvode_mem, double t0, N_Vector y0, double reltol, N_Vector abstol)
int CvodeInterface::reInit (const double& t0)
{
    if (mCVODE_Memory == NULL)
    {
        return CV_SUCCESS;
    }

    int result = CVodeReInit(mCVODE_Memory,  t0, mStateVector);

    if (result != CV_SUCCESS)
    {
        return result;
    }

    return CVodeSVtolerances(mCVODE_Memory, mRelTol, mAbstolArray);
}

double CvodeInterface::oneStep(const double& _timeStart, const double& hstep)
{
    Log(lDebug3)<<"---------------------------------------------------";
    Log(lDebug3)<<"--- O N E     S T E P      ( "<<mOneStepCount<< " ) ";
    Log(lDebug3)<<"---------------------------------------------------";

    mOneStepCount++;
    mCount = 0;

    double timeEnd = 0.0;
    double timeStart = _timeStart;
    double tout = timeStart + hstep;
    int strikes = 3;
    try
    {
        // here we stop for a too small timestep ... this seems troublesome to me ...
        while (tout - timeEnd > 1E-16)
        {
            if (hstep < 1E-16)
            {
                return tout;
            }

            // here we bail in case we have no ODEs set up with CVODE ... though we should
            // still at least evaluate the model function
            if (!haveVariables() && mModel->getNumEvents() == 0)
            {
                mModel->convertToAmounts();
                mModel->evalModel(tout, 0, 0);
                return tout;
            }

            if (mLastTimeValue > timeStart)
            {
                reStart(timeStart, mModel);
            }

            double nextTargetEndTime = tout;
            if (mAssignmentTimes.size() > 0 && mAssignmentTimes[0] < nextTargetEndTime)
            {
                nextTargetEndTime = mAssignmentTimes[0];
                mAssignmentTimes.erase(mAssignmentTimes.begin());
            }

            int nResult = CVode(mCVODE_Memory, nextTargetEndTime,  mStateVector, &timeEnd, CV_NORMAL);

            if (nResult == CV_ROOT_RETURN && mFollowEvents)
            {
                Log(lDebug1)<<("---------------------------------------------------");
                Log(lDebug1)<<"--- E V E N T      ( " << mOneStepCount << " ) ";
                Log(lDebug1)<<("---------------------------------------------------");

                bool tooCloseToStart = fabs(timeEnd - mLastEvent) > mRelTol;

                if(tooCloseToStart)
                {
                    strikes =  3;
                }
                else
                {
                    strikes--;
                }

                if (tooCloseToStart || strikes > 0)
                {
                    handleRootsFound(timeEnd, tout);
                    reStart(timeEnd, mModel);
                    mLastEvent = timeEnd;
                }
            }
            else if (nResult == CV_SUCCESS || !mFollowEvents)
            {
                //mTheModel->resetEvents();
                mModel->setTime(tout);
                assignResultsToModel();
            }
            else
            {
                handleCVODEError(nResult);
            }

            mLastTimeValue = timeEnd;

            try
            {
                mModel->testConstraints();
            }
            catch (const Exception& e)
            {
                Log(lWarning)<<"Constraint Violated at time = " + toString(timeEnd)<<": " + e.Message();

            }

            assignPendingEvents(timeEnd, tout);

            if (tout - timeEnd > 1E-16)
            {
                timeStart = timeEnd;
            }
            Log(lDebug3)<<"tout: "<<tout<<gTab<<"timeEnd: "<<timeEnd;
        }
        return (timeEnd);
    }
    catch(const Exception& ex)
    {
        Log(lError)<<"Problem in OneStep: "<<ex.getMessage()<<endl;
        initializeCVODEInterface(mModel);    //tk says ??? tk
        throw;
    }
}

void ModelFcn(int n, double time, double* y, double* ydot, void* userData)
{
    CvodeInterface* cvInstance = (CvodeInterface*) userData;
    if(!cvInstance)
    {
        Log(lError)<<"Problem in CVode Model Function!";
        return;
    }

    ExecutableModel *model = cvInstance->getModel();

    model->pushState();

    Log(Logger::PRIO_TRACE) << __FUNC__ << endl;

    model->evalModel(time, y, ydot);

    cvInstance->mCount++;

    model->popState();
}

void EventFcn(double time, double* y, double* gdot, void* userData)
{
    CvodeInterface* cvInstance = (CvodeInterface*) userData;
    if(!cvInstance)
    {
        Log(lError)<<"Problem in CVode Model Function";
        return;
    }

    ExecutableModel *model = cvInstance->getModel();

    model->pushState();

    model->evalModel(time, 0, 0);
    cvInstance->assignResultsToModel();

    model->evalEvents(time, 0);

    for(int i = 0; i < model->getNumEvents(); i++)
    {
        gdot[i] = model->getModelData().eventTests[i];
    }

    cvInstance->mRootCount++;

    model->popState();
}

bool CvodeInterface::haveVariables()
{
    return (mStateVectorSize > 0) ? true : false;
}

void CvodeInterface::initializeCVODEInterface(ExecutableModel *oModel)
{
    if(!oModel)
    {
        throw CVODEException("Fatal Error while initializing CVODE");
    }

    try
    {
        mModel = oModel;
        mStateVectorSize = oModel->getStateVector(0);

        if (mStateVectorSize > 0)
        {
            mStateVector =     N_VNew_Serial(mStateVectorSize);
            mAbstolArray = N_VNew_Serial(mStateVectorSize);
            for (int i = 0; i < mStateVectorSize; i++)
            {
                SetVector((N_Vector) mAbstolArray, i, mDefaultAbsTol);
            }

            assignNewVector(oModel, true);

            mCVODE_Memory = (void*) CVodeCreate(CV_BDF, CV_NEWTON);
            //SetMaxOrder(mCVODE_Memory, MaxBDFOrder);
            if(mCVODE_Memory)
            {
                CVodeSetMaxOrd(mCVODE_Memory, mMaxBDFOrder);
                CVodeSetInitStep(mCVODE_Memory, mInitStep);
                CVodeSetMinStep(mCVODE_Memory, mMinStep);
                CVodeSetMaxStep(mCVODE_Memory, mMaxStep);
                CVodeSetMaxNumSteps(mCVODE_Memory, mMaxNumSteps);
            }

            int errCode = allocateCvodeMem();

            if (errCode < 0)
            {
                handleCVODEError(errCode);
            }

            if (oModel->getNumEvents() > 0)
            {
                errCode = rootInit(oModel->getNumEvents());//, EventFcn, gdata);
                Log(lDebug2)<<"CVRootInit executed.....";
            }

               errCode = CVDense(mCVODE_Memory, mStateVectorSize); // int = size of systems


            if (errCode < 0)
            {
                handleCVODEError(errCode);
            }

            oModel->resetEvents();
        }
        else if (mModel->getNumEvents() > 0)
        {
            int allocated = 1;
            mStateVector         = N_VNew_Serial(allocated);
            mAbstolArray     = N_VNew_Serial(allocated);
            SetVector(mStateVector, 0, 10);
            SetVector(mAbstolArray, 0, mDefaultAbsTol);

            mCVODE_Memory = (void*) CVodeCreate(CV_BDF, CV_NEWTON);
            CVodeSetMaxOrd(mCVODE_Memory, mMaxBDFOrder);
            CVodeSetMaxNumSteps(mCVODE_Memory, mMaxNumSteps);

            int errCode = allocateCvodeMem();
            if (errCode < 0)
            {
                handleCVODEError(errCode);
            }

            if (oModel->getNumEvents() > 0)
            {
                errCode = rootInit(oModel->getNumEvents());
                Log(lDebug2)<<"CVRootInit executed.....";
            }

            errCode = CVDense(mCVODE_Memory, allocated);
            if (errCode < 0)
            {
                handleCVODEError(errCode);
            }

            oModel->resetEvents();
        }
    }
    catch (const Exception& ex)
    {
        Log(lError)<<"Fatal Error while initializing CVODE: " << ex.getMessage();
        throw CVODEException("Fatal Error while initializing CVODE");
    }
}

void CvodeInterface::assignPendingEvents(const double& timeEnd, const double& tout)
{
    for (int i = (int) mAssignments.size() - 1; i >= 0; i--)
    {
        if (timeEnd >= mAssignments[i].GetTime())
        {
            mModel->setTime(tout);
            assignResultsToModel();
            mModel->convertToConcentrations();
            mModel->updateDependentSpeciesValues();
            mAssignments[i].AssignToModel();

            if (mModel->getConservedSumChanged())
            {
                 mModel->computeConservedTotals();
            }

            mModel->convertToAmounts();
            mModel->evalModel(timeEnd, 0, 0);
            reStart(timeEnd, mModel);
            mAssignments.erase(mAssignments.begin() + i);
        }
    }
}

vector<int> CvodeInterface::retestEvents(const double& timeEnd, const vector<int>& handledEvents, vector<int>& removeEvents)
{
    return retestEvents(timeEnd, handledEvents, false, removeEvents);
}

vector<int> CvodeInterface::retestEvents(const double& timeEnd, vector<int>& handledEvents, const bool& assignOldState)
{
    vector<int> removeEvents;
    return retestEvents(timeEnd, handledEvents, assignOldState, removeEvents);
}

vector<int> CvodeInterface::retestEvents(const double& timeEnd, const vector<int>& handledEvents, const bool& assignOldState, vector<int>& removeEvents)
{
    vector<int> result;
//    vector<int> removeEvents;// = new vector<int>();    //Todo: this code was like this originally.. which removeEvents to use???

    if (mModel->getConservedSumChanged())
    {
        mModel->computeConservedTotals();
    }

    mModel->convertToAmounts();
    mModel->evalModel(timeEnd, 0, 0);

    // copy original evenStatusArray
    vector<bool> eventStatusArray(mModel->getModelData().eventStatusArray,
            mModel->getModelData().eventStatusArray +
            mModel->getModelData().numEvents / sizeof(bool));

    mModel->pushState();

    mModel->evalEvents(timeEnd, 0);

    for (int i = 0; i < mModel->getNumEvents(); i++)
    {
        bool containsI = (std::find(handledEvents.begin(), handledEvents.end(), i) != handledEvents.end()) ? true : false;
        if (mModel->getModelData().eventStatusArray[i] == true && eventStatusArray[i] == false && !containsI)
        {
            result.push_back(i);
        }

        if (mModel->getModelData().eventStatusArray[i] == false && eventStatusArray[i] == true && !mModel->getModelData().eventPersistentType[i])
        {
            removeEvents.push_back(i);
        }
    }

    mModel->popState(assignOldState ? 0 : ExecutableModel::PopDiscard);

    return result;
}

void CvodeInterface::handleRootsFound(double &timeEnd, const double& tout)
{
    vector<int> rootsFound(mModel->getNumEvents());

    // Create some space for the CVGetRootInfo call
    int* _rootsFound = new int[mModel->getNumEvents()];
    CVodeGetRootInfo(mCVODE_Memory, _rootsFound);
    copyCArrayToStdVector(_rootsFound, rootsFound, mModel->getNumEvents());
    delete [] _rootsFound;
    handleRootsForTime(timeEnd, rootsFound);
}

void CvodeInterface::testRootsAtInitialTime()
{
    vector<int> dummy;
    vector<int> events = retestEvents(0, dummy, true); //Todo: dummy is passed but is not used..?

    if (events.size() > 0)
    {
        vector<int> rootsFound(mModel->getNumEvents());//         = new int[mTheModel->getNumEvents];
        vector<int>::iterator iter;
        for(iter = rootsFound.begin(); iter != rootsFound.end(); iter ++)
        {
            (*iter) = 1;
        }
        handleRootsForTime(0, rootsFound);
    }
}

void CvodeInterface::removePendingAssignmentForIndex(const int& eventIndex)
{
    for (int j = (int) mAssignments.size() - 1; j >= 0; j--)
    {
        if (mAssignments[j].GetIndex() == eventIndex)
        {
            mAssignments.erase(mAssignments.begin() + j);
        }
    }
}

void CvodeInterface::sortEventsByPriority(vector<rr::Event>& firedEvents)
{
    if ((firedEvents.size() > 1))
    {
        Log(lDebug3)<<"Sorting event priorities";
        for(int i = 0; i < firedEvents.size(); i++)
        {
            firedEvents[i].SetPriority(mModel->getModelData().eventPriorities[firedEvents[i].GetID()]);
            Log(lDebug3)<<firedEvents[i];
        }
        sort(firedEvents.begin(), firedEvents.end(), SortByPriority());

        Log(lDebug3)<<"After sorting event priorities";
        for(int i = 0; i < firedEvents.size(); i++)
        {
            Log(lDebug3)<<firedEvents[i];
        }
    }
}

void CvodeInterface::sortEventsByPriority(vector<int>& firedEvents)
{
    if (firedEvents.size() > 1)
    {
        mModel->computeEventPriorites();
        vector<rr::Event> dummy;
        for(int i = 0; i < firedEvents.size(); i++)
        {
            Event event(firedEvents[i]);
            dummy.push_back(event);
        }

        Log(lDebug3)<<"Sorting event priorities";
        for(int i = 0; i < firedEvents.size(); i++)
        {
            Event &event = dummy[i];
            event.SetPriority(mModel->getModelData().eventPriorities[event.GetID()]);
            Log(lDebug3) << event;
        }
        sort(dummy.begin(), dummy.end(), SortByPriority());

        for(int i = 0; i < firedEvents.size(); i++)
        {
            firedEvents[i] = dummy[i].GetID();
        }

        Log(lDebug3)<<"After sorting event priorities";
        for(int i = 0; i < firedEvents.size(); i++)
        {
            Log(lDebug3)<<firedEvents[i];
        }
    }
}

void CvodeInterface::handleRootsForTime(const double& timeEnd, vector<int>& rootsFound)
{
    assignResultsToModel();
    mModel->convertToConcentrations();
    mModel->updateDependentSpeciesValues();
    mModel->evalEvents(timeEnd, 0);

    vector<int> firedEvents;
    map<int, double* > preComputedAssignments;

    for (int i = 0; i < mModel->getNumEvents(); i++)
    {
        // We only fire an event if we transition from false to true
        if (rootsFound[i] == 1)
        {
            if (mModel->getModelData().eventStatusArray[i])
            {
                firedEvents.push_back(i);
                if (mModel->getModelData().eventType[i])
                {
                    preComputedAssignments[i] = mModel->getModelData().computeEventAssignments[i](&(mModel->getModelData()));
                }
            }
        }
        else
        {
            // if the trigger condition is not supposed to be persistent, remove the event from the firedEvents list;
            if (!mModel->getModelData().eventPersistentType[i])
            {
                removePendingAssignmentForIndex(i);
            }
        }
    }

    vector<int> handled;
    while (firedEvents.size() > 0)
    {
        sortEventsByPriority(firedEvents);
        // Call event assignment if the eventstatus flag for the particular event is false
        for (u_int i = 0; i < firedEvents.size(); i++)
        {
            int currentEvent = firedEvents[i];//.GetID();

            // We only fire an event if we transition from false to true
            mModel->getModelData().previousEventStatusArray[currentEvent] = mModel->getModelData().eventStatusArray[currentEvent];
            double eventDelay = mModel->getModelData().eventDelays[currentEvent](&(mModel->getModelData()));
            if (eventDelay == 0)
            {
                if (mModel->getModelData().eventType[currentEvent] && preComputedAssignments.count(currentEvent) > 0)
                {
                    mModel->getModelData().performEventAssignments[currentEvent](&(mModel->getModelData()), preComputedAssignments[currentEvent]);
                }
                else
                {
                    mModel->getModelData().eventAssignments[currentEvent]();
                }

                handled.push_back(currentEvent);
                vector<int> removeEvents;
                vector<int> additionalEvents = retestEvents(timeEnd, handled, removeEvents);

                std::copy (additionalEvents.begin(), additionalEvents.end(), firedEvents.end());

                for (int j = 0; j < additionalEvents.size(); j++)
                {
                    int newEvent = additionalEvents[j];
                    if (mModel->getModelData().eventType[newEvent])
                    {
                        preComputedAssignments[newEvent] = mModel->getModelData().computeEventAssignments[newEvent](&(mModel->getModelData()));
                    }
                }

                mModel->getModelData().eventStatusArray[currentEvent] = false;
                Log(lDebug3)<<"Fired Event with ID:"<<currentEvent;
                firedEvents.erase(firedEvents.begin() + i);

                for (int i = 0; i < removeEvents.size(); i++)
                {
                    int item = removeEvents[i];
                    if (find(firedEvents.begin(), firedEvents.end(), item) != firedEvents.end())
                    {
                        firedEvents.erase(find(firedEvents.begin(), firedEvents.end(), item));
                        removePendingAssignmentForIndex(item);
                    }
                }

                break;
            }
            else
            {
                if (find(mAssignmentTimes.begin(), mAssignmentTimes.end(), timeEnd + eventDelay) == mAssignmentTimes.end())
                {
                    mAssignmentTimes.push_back(timeEnd + eventDelay);
                }

                PendingAssignment *pending = new PendingAssignment( &(mModel->getModelData()),
                                                                    timeEnd + eventDelay,
                                                                    mModel->getModelData().computeEventAssignments[currentEvent],
                                                                    mModel->getModelData().performEventAssignments[currentEvent],
                                                                    mModel->getModelData().eventType[currentEvent],
                                                                    currentEvent);

                if (mModel->getModelData().eventType[currentEvent] && preComputedAssignments.count(currentEvent) == 1)
                {
                    pending->ComputedValues = preComputedAssignments[currentEvent];
                }

                mAssignments.push_back(*pending);
                mModel->getModelData().eventStatusArray[currentEvent] = false;
                firedEvents.erase(firedEvents.begin() + i);
                break;
            }
        }
    }

    if (mModel->getConservedSumChanged())
    {
        mModel->computeConservedTotals();
    }
    mModel->convertToAmounts();

    mModel->evalModel(timeEnd, 0, 0);

    mModel->getStateVector(NV_DATA_S(mStateVector));

    reInit(timeEnd);//, mAmounts, mRelTol, mAbstolArray);
    sort(mAssignmentTimes.begin(), mAssignmentTimes.end());
}

void CvodeInterface::assignResultsToModel()
{
    mModel->setStateVector(NV_DATA_S(mStateVector));
}

void CvodeInterface::assignNewVector(ExecutableModel *model)
{
    assignNewVector(model, false);
}

// Restart the simulation using a different initial condition
void CvodeInterface::assignNewVector(ExecutableModel *oModel,
        bool assignNewTolerances)
{
    if (mStateVector == 0)
    {
        if (oModel && oModel->getStateVector(0) != 0)
        {
            Log(lWarning) << "Attempting to assign non-zero state vector to "
                    "zero length state vector in " << __FUNC__;
        }
        return;
    }

    if (oModel->getStateVector(0) > NV_LENGTH_S(mStateVector))
    {
        stringstream msg;
        msg << "attempt to assign different length data to existing state vector, ";
        msg << "new data has " << oModel->getStateVector(0) << " elements and ";
        msg << "existing state vector has " << NV_LENGTH_S(mStateVector);

        poco_error(getLogger(), msg.str());

        throw CVODEException(msg.str());
    }

    oModel->getStateVector(NV_DATA_S(mStateVector));

    if (assignNewTolerances)
    {
        double dMin = mAbsTol;

        for (int i = 0; i < NV_LENGTH_S(mStateVector); ++i)
        {
            double tmp = NV_DATA_S(mStateVector)[i] / 1000.;
            if (tmp > 0 && tmp < dMin)
            {
                dMin = tmp;
            }
        }

        for (int i = 0; i < NV_LENGTH_S(mStateVector); ++i)
        {
            setAbsTolerance(i, dMin);
        }

        // TODO: events are bizarre, need to clean them up eventually
        if (!haveVariables() && mModel->getNumEvents() > 0)
        {
            setAbsTolerance(0, dMin);
            SetVector(mStateVector, 0, 1.0);
        }

        Log(lDebug1)<<"Set tolerance to: "<<setprecision(16)<< dMin;
    }
}

void CvodeInterface::setAbsTolerance(int index, double dValue)
{
    double dTolerance = dValue;
    if (dValue > 0 && mAbsTol > dValue)
    {
        dTolerance = dValue;
    }
    else
    {
        dTolerance = mAbsTol;
    }

    SetVector(mAbstolArray, index, dTolerance);
}

void CvodeInterface::reStart(double timeStart, ExecutableModel* model)
{
    assignNewVector(model);

    if(mCVODE_Memory)
    {
        CVodeSetInitStep(mCVODE_Memory, mInitStep);
        CVodeSetMinStep(mCVODE_Memory, mMinStep);
        CVodeSetMaxStep(mCVODE_Memory, mMaxStep);
        reInit(timeStart);
    }
}


void CvodeInterface::handleCVODEError(const int& errCode)
{
    if (errCode < 0)
    {
        Log(lError) << "**************** Error in RunCVode: "
                << errCode << " ****************************" << endl;
        throw(Exception("Error in CVODE...!"));
    }
}

// Sets the value of an element in a N_Vector object
void SetVector (N_Vector v, int Index, double Value)
{
    double *data = NV_DATA_S(v);
    data[Index] = Value;
}

double GetVector (N_Vector v, int Index)
{
    double *data = NV_DATA_S(v);
    return data[Index];
}

// Cvode calls this to compute the dy/dts. This routine in turn calls the
// model function which is located in the host application.
int InternalFunctionCall(realtype t, N_Vector cv_y, N_Vector cv_ydot, void *f_data)
{
    // Calls the callBackModel here
    ModelFcn(NV_LENGTH_S(cv_y), t, NV_DATA_S (cv_y), NV_DATA_S(cv_ydot), f_data);
    return CV_SUCCESS;
}

//int (*CVRootFn)(realtype t, N_Vector y, realtype *gout, void *user_data)
// Cvode calls this to check for event changes
int InternalRootCall (realtype t, N_Vector y, realtype *gout, void *g_data)
{
    EventFcn(t, NV_DATA_S (y), gout, g_data);
    return CV_SUCCESS;
}

}


