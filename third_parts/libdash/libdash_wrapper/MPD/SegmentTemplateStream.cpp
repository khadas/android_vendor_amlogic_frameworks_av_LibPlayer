/*
 * SegmentTemplateStream.cpp
 *****************************************************************************
 * Copyright (C) 2013, bitmovin Softwareentwicklung OG, All Rights Reserved
 *
 * Email: libdash-dev@vicky.bitmovin.net
 *
 * This source code and its use and distribution, is subject to the terms
 * and conditions of the applicable license agreement.
 *****************************************************************************/

#include "SegmentTemplateStream.h"

using namespace dash::mpd;
using namespace libdash::framework::mpd;

SegmentTemplateStream::SegmentTemplateStream            (IMPD *mpd, IPeriod *period, IAdaptationSet *adaptationSet, IRepresentation *representation) :
                       AbstractRepresentationStream     (mpd, period, adaptationSet, representation)
{
    this->baseUrls          = BaseUrlResolver::ResolveBaseUrl(mpd, period, adaptationSet, 0, 0, 0);
    this->segmentTemplate   = FindSegmentTemplate();
    CalculateSegmentStartTimes();
}
SegmentTemplateStream::~SegmentTemplateStream           ()
{
}
ISegment*                   SegmentTemplateStream::GetInitializationSegment     ()
{
    if (this->segmentTemplate->GetInitialization())
        return this->segmentTemplate->GetInitialization()->ToSegment(baseUrls);

    return this->segmentTemplate->ToInitializationSegment(baseUrls, representation->GetId(), representation->GetBandwidth());
}
ISegment*                   SegmentTemplateStream::GetIndexSegment              (size_t segmentNumber)
{
    /* time-based template */
    if (this->segmentTemplate->GetSegmentTimeline())
    {
        if (this->segmentStartTimes.size() > segmentNumber)
            return this->segmentTemplate->GetIndexSegmentFromTime(baseUrls, representation->GetId(), representation->GetBandwidth(), this->segmentStartTimes.at(segmentNumber));

        return NULL;
    }

    /* number-based template */
    return this->segmentTemplate->GetIndexSegmentFromNumber(baseUrls, representation->GetId(), representation->GetBandwidth(),
                                                                this->segmentTemplate->GetStartNumber() + segmentNumber);
}
ISegment*                   SegmentTemplateStream::GetMediaSegment              (size_t segmentNumber)
{
    /* time-based template */
    if (this->segmentTemplate->GetSegmentTimeline())
    {
        if (this->segmentStartTimes.size() > segmentNumber)
            return this->segmentTemplate->GetMediaSegmentFromTime(baseUrls, representation->GetId(), representation->GetBandwidth(), this->segmentStartTimes.at(segmentNumber),segmentNumber);

        return NULL;
    }

    /* number-based template */
    return this->segmentTemplate->GetMediaSegmentFromNumber(baseUrls, representation->GetId(), representation->GetBandwidth(),
                                                            this->segmentTemplate->GetStartNumber() + segmentNumber,segmentNumber);

}
ISegment*                   SegmentTemplateStream::GetBitstreamSwitchingSegment ()
{
    if (this->segmentTemplate->GetBitstreamSwitching())
        return this->segmentTemplate->GetBitstreamSwitching()->ToSegment(baseUrls);

    return this->segmentTemplate->ToBitstreamSwitchingSegment(baseUrls, representation->GetId(), representation->GetBandwidth());
}
RepresentationStreamType    SegmentTemplateStream::GetStreamType                ()
{
    return SegmentList;
}
uint32_t                    SegmentTemplateStream::GetSize                      ()
{
    if (!this->segmentStartTimes.empty())
        return this->segmentStartTimes.size();

    uint32_t numberOfSegments          = 0;
    double   mediaPresentationDuration = 0;

    if (this->mpd->GetType() == "static")
    {
        mediaPresentationDuration = TimeResolver::GetDurationInSec(this->mpd->GetMediaPresentationDuration());

        numberOfSegments = (uint32_t) ceil(mediaPresentationDuration / (this->segmentTemplate->GetDuration() / this->segmentTemplate->GetTimescale()));
    }
    else
    {
        // TODO: set number to 1 when no timeline exits, maybe need to change.
        numberOfSegments = 1;
    }

    return numberOfSegments;
}
uint32_t                    SegmentTemplateStream::GetAverageSegmentDuration    ()
{
    /* TODO calculate average segment durations for SegmentTimeline */
    return this->segmentTemplate->GetDuration();
}
uint32_t                    SegmentTemplateStream::GetSegmentTimescale       ()
{
    uint32_t scale = this->segmentTemplate->GetTimescale();
    if (scale > 1) {
        return scale;
    } else {
        return 90000;
    }
}

uint64_t    SegmentTemplateStream::GetSegmentStartTimeByOffset(uint32_t offset)
{
    if (!this->segmentTemplate->GetSegmentTimeline() || (offset >= this->segmentStartTimes.size()))
        return 0;

    return this->segmentStartTimes.at(offset);
}

uint32_t    SegmentTemplateStream::GetStartNumber()
{
    return this->segmentTemplate->GetStartNumber();
}

bool    SegmentTemplateStream::HasTimeLineOrNot()
{
    return (this->segmentTemplate->GetSegmentTimeline() != NULL);
}

int32_t    SegmentTemplateStream::GetSegmentOffsetByTime(uint64_t timeToFind)
{
    if (!this->segmentTemplate->GetSegmentTimeline() || !this->segmentStartTimes.size())
        return -2;

    if (timeToFind < this->segmentTemplate->GetSegmentTimeline()->GetTimelines().at(0)->GetStartTime()) {
        return -1;
    }

    int32_t ret = -2;
    for (size_t i = 0; i < this->segmentStartTimes.size(); i++) {
        if (this->segmentStartTimes.at(i) == timeToFind) {
            ret = i;
            break;
        }
    }

    return ret;
}

ISegmentTemplate*           SegmentTemplateStream::FindSegmentTemplate          ()
{
    if (this->representation->GetSegmentTemplate())
        return this->representation->GetSegmentTemplate();

    if (this->adaptationSet->GetSegmentTemplate())
        return this->adaptationSet->GetSegmentTemplate();

    if (this->period->GetSegmentTemplate())
        return this->period->GetSegmentTemplate();

    return NULL;
}
void                        SegmentTemplateStream::CalculateSegmentStartTimes   ()
{
    if (!this->segmentTemplate->GetSegmentTimeline())
        return;

    size_t   numOfTimelines = 0;
    uint64_t segStartTime   = 0;
    uint64_t segDuration    = 0;
    uint64_t accTime = 0;
    size_t   repeatCount    = 0;

    numOfTimelines      = this->segmentTemplate->GetSegmentTimeline()->GetTimelines().size();

    for (size_t i = 0; i < numOfTimelines; i++)
    {
        repeatCount     = this->segmentTemplate->GetSegmentTimeline()->GetTimelines().at(i)->GetRepeatCount();
        segStartTime    = this->segmentTemplate->GetSegmentTimeline()->GetTimelines().at(i)->GetStartTime();
        segDuration     = this->segmentTemplate->GetSegmentTimeline()->GetTimelines().at(i)->GetDuration();
        if (segStartTime) {
            accTime = segStartTime;
        }

        if (repeatCount > 0)
        {
            for (size_t j = 0; j <= repeatCount; j++)
            {
                if (segStartTime > 0)
                {
                    this->segmentStartTimes.push_back(segStartTime + segDuration * j);
                }
                else
                {
                    /* TODO: implement if SegmentTemplate.SegmentTimeline.S@t is not specified */
                }
            }
        }
        else
        {
            this->segmentStartTimes.push_back(accTime);
        }

        accTime += segDuration;
    }
}
