/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2008 Jose Aparicio
 Copyright (C) 2014 Peter Caspers

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include <ql/quantlib.hpp>

#include <boost/timer.hpp>
#include <iostream>
#include <iomanip>

#include <boost/make_shared.hpp>
#include <memory>

using namespace std;
using namespace QuantLib;

#if defined(QL_ENABLE_SESSIONS)
namespace QuantLib {

    Integer sessionId() { return 0; }
}
#endif

void example01() {

    boost::timer timer;
    std::cout << std::endl;

    /*********************
     ***  MARKET DATA  ***
     *********************/

    Calendar calendar = TARGET();
    Date todaysDate(15, May, 2007);
    // must be a business day
    todaysDate = calendar.adjust(todaysDate);

    Settings::instance().evaluationDate() = todaysDate;

    // dummy curve
    boost::shared_ptr<Quote> flatRate(new SimpleQuote(0.01));
    Handle<YieldTermStructure> tsCurve(
        boost::shared_ptr<FlatForward>(new FlatForward(
            todaysDate, Handle<Quote>(flatRate), Actual365Fixed())));

    /*
      In Lehmans Brothers "guide to exotic credit derivatives"
      p. 32 there's a simple case, zero flat curve with a flat CDS
      curve with constant market spreads of 150 bp and RR = 50%
      corresponds to a flat 3% hazard rate. The implied 1-year
      survival probability is 97.04% and the 2-years is 94.18%
    */

    // market
    Real recovery_rate = 0.5;
    Real quoted_spreads[] = { 0.0150, 0.0150, 0.0150, 0.0150 };
    vector<Period> tenors;
    tenors.push_back(3 * Months);
    tenors.push_back(6 * Months);
    tenors.push_back(1 * Years);
    tenors.push_back(2 * Years);
    vector<Date> maturities;
    for (Size i = 0; i < 4; i++) {
        maturities.push_back(
            calendar.adjust(todaysDate + tenors[i], Following));
    }

    std::vector<boost::shared_ptr<DefaultProbabilityHelper> > instruments;
    for (Size i = 0; i < 4; i++) {
        instruments.push_back(boost::shared_ptr<DefaultProbabilityHelper>(
            new SpreadCdsHelper(Handle<Quote>(boost::shared_ptr<Quote>(
                                    new SimpleQuote(quoted_spreads[i]))),
                                tenors[i], 0, calendar, Quarterly, Following,
                                DateGeneration::TwentiethIMM, Actual365Fixed(),
                                recovery_rate, tsCurve)));

    }

    // Bootstrap hazard rates
    boost::shared_ptr<PiecewiseDefaultCurve<HazardRate, BackwardFlat> >
    hazardRateStructure(new PiecewiseDefaultCurve<HazardRate, BackwardFlat>(
        todaysDate, instruments, Actual365Fixed()));
    vector<pair<Date, Real> > hr_curve_data = hazardRateStructure->nodes();

    cout << "Calibrated hazard rate values: " << endl;
    for (Size i = 0; i < hr_curve_data.size(); i++) {
        cout << "hazard rate on " << hr_curve_data[i].first << " is "
             << hr_curve_data[i].second << endl;
    }
    cout << endl;

    cout << "Some survival probability values: " << endl;
    cout << "1Y survival probability: "
         << io::percent(hazardRateStructure->survivalProbability(todaysDate +
                                                                 1 * Years))
         << endl << "               expected: " << io::percent(0.9704) << endl;
    cout << "2Y survival probability: "
         << io::percent(hazardRateStructure->survivalProbability(todaysDate +
                                                                 2 * Years))
         << endl << "               expected: " << io::percent(0.9418) << endl;

    cout << endl << endl;

    // reprice instruments
    Real nominal = 1000000.0;
    Handle<DefaultProbabilityTermStructure> probability(hazardRateStructure);
    boost::shared_ptr<PricingEngine> engine(
        new MidPointCdsEngine(probability, recovery_rate, tsCurve));

    Schedule cdsSchedule = MakeSchedule()
                               .from(todaysDate)
                               .to(maturities[0])
                               .withFrequency(Quarterly)
                               .withCalendar(calendar)
                               .withTerminationDateConvention(Unadjusted)
                               .withRule(DateGeneration::TwentiethIMM);
    CreditDefaultSwap cds_3m(Protection::Seller, nominal, quoted_spreads[0],
                             cdsSchedule, Following, Actual365Fixed());

    cdsSchedule = MakeSchedule()
                      .from(todaysDate)
                      .to(maturities[1])
                      .withFrequency(Quarterly)
                      .withCalendar(calendar)
                      .withTerminationDateConvention(Unadjusted)
                      .withRule(DateGeneration::TwentiethIMM);
    CreditDefaultSwap cds_6m(Protection::Seller, nominal, quoted_spreads[1],
                             cdsSchedule, Following, Actual365Fixed());

    cdsSchedule = MakeSchedule()
                      .from(todaysDate)
                      .to(maturities[2])
                      .withFrequency(Quarterly)
                      .withCalendar(calendar)
                      .withTerminationDateConvention(Unadjusted)
                      .withRule(DateGeneration::TwentiethIMM);
    CreditDefaultSwap cds_1y(Protection::Seller, nominal, quoted_spreads[2],
                             cdsSchedule, Following, Actual365Fixed());

    cdsSchedule = MakeSchedule()
                      .from(todaysDate)
                      .to(maturities[3])
                      .withFrequency(Quarterly)
                      .withCalendar(calendar)
                      .withTerminationDateConvention(Unadjusted)
                      .withRule(DateGeneration::TwentiethIMM);
    CreditDefaultSwap cds_2y(Protection::Seller, nominal, quoted_spreads[3],
                             cdsSchedule, Following, Actual365Fixed());

    cds_3m.setPricingEngine(engine);
    cds_6m.setPricingEngine(engine);
    cds_1y.setPricingEngine(engine);
    cds_2y.setPricingEngine(engine);

    cout << "Repricing of quoted CDSs employed for calibration: " << endl;
    cout << "3M fair spread: " << io::rate(cds_3m.fairSpread()) << endl
         << "   NPV:         " << cds_3m.NPV() << endl
         << "   default leg: " << cds_3m.defaultLegNPV() << endl
         << "   coupon leg:  " << cds_3m.couponLegNPV() << endl << endl;

    cout << "6M fair spread: " << io::rate(cds_6m.fairSpread()) << endl
         << "   NPV:         " << cds_6m.NPV() << endl
         << "   default leg: " << cds_6m.defaultLegNPV() << endl
         << "   coupon leg:  " << cds_6m.couponLegNPV() << endl << endl;

    cout << "1Y fair spread: " << io::rate(cds_1y.fairSpread()) << endl
         << "   NPV:         " << cds_1y.NPV() << endl
         << "   default leg: " << cds_1y.defaultLegNPV() << endl
         << "   coupon leg:  " << cds_1y.couponLegNPV() << endl << endl;

    cout << "2Y fair spread: " << io::rate(cds_2y.fairSpread()) << endl
         << "   NPV:         " << cds_2y.NPV() << endl
         << "   default leg: " << cds_2y.defaultLegNPV() << endl
         << "   coupon leg:  " << cds_2y.couponLegNPV() << endl << endl;

    cout << endl << endl;

    Real seconds = timer.elapsed();
    Integer hours = Integer(seconds / 3600);
    seconds -= hours * 3600;
    Integer minutes = Integer(seconds / 60);
    seconds -= minutes * 60;
    cout << "Run completed in ";
    if (hours > 0)
        cout << hours << " h ";
    if (hours > 0 || minutes > 0)
        cout << minutes << " m ";
    cout << fixed << setprecision(0) << seconds << " s" << endl;
}

void example02() {

    Date evaluationDate = Date(14, January, 2014);

    Settings::instance().evaluationDate() = evaluationDate;

    // set up ISDA IR curve helpers

    boost::shared_ptr<DepositRateHelper> dp1m =
        boost::make_shared<DepositRateHelper>(0.02, 1 * Months, 2,
                                              WeekendsOnly(), ModifiedFollowing,
                                              false, Actual360());
    boost::shared_ptr<DepositRateHelper> dp2m =
        boost::make_shared<DepositRateHelper>(0.02, 2 * Months, 2,
                                              WeekendsOnly(), ModifiedFollowing,
                                              false, Actual360());
    boost::shared_ptr<DepositRateHelper> dp3m =
        boost::make_shared<DepositRateHelper>(0.02, 3 * Months, 2,
                                              WeekendsOnly(), ModifiedFollowing,
                                              false, Actual360());
    boost::shared_ptr<DepositRateHelper> dp6m =
        boost::make_shared<DepositRateHelper>(0.02, 6 * Months, 2,
                                              WeekendsOnly(), ModifiedFollowing,
                                              false, Actual360());
    boost::shared_ptr<DepositRateHelper> dp9m =
        boost::make_shared<DepositRateHelper>(0.02, 9 * Months, 2,
                                              WeekendsOnly(), ModifiedFollowing,
                                              false, Actual360());
    boost::shared_ptr<DepositRateHelper> dp12m =
        boost::make_shared<DepositRateHelper>(0.02, 12 * Months, 2,
                                              WeekendsOnly(), ModifiedFollowing,
                                              false, Actual360());

    // intentionally we do not provide a fixing for the euribor index used for
    // bootstrapping in order to be compliant with the ISDA specification
    boost::shared_ptr<IborIndex> euribor6m =
        boost::make_shared<Euribor>(Euribor(6 * Months));

// check if indexed coupon is defined (it should not to be 100% consistent with
// the ISDA spec)
#ifdef QL_INDEXED_COUPON
    std::cout << "Warning: QL_INDEXED_COUPON is defined, which is not "
              << "precisely consistent with the specification of the ISDA rate "
              << "curve." << std::endl;
#endif

    boost::shared_ptr<SwapRateHelper> sw2y = boost::make_shared<SwapRateHelper>(
        0.02, 2 * Years, WeekendsOnly(), Annual, ModifiedFollowing, Thirty360(),
        euribor6m);
    boost::shared_ptr<SwapRateHelper> sw3y = boost::make_shared<SwapRateHelper>(
        0.02, 3 * Years, WeekendsOnly(), Annual, ModifiedFollowing, Thirty360(),
        euribor6m);
    boost::shared_ptr<SwapRateHelper> sw4y = boost::make_shared<SwapRateHelper>(
        0.02, 4 * Years, WeekendsOnly(), Annual, ModifiedFollowing, Thirty360(),
        euribor6m);
    boost::shared_ptr<SwapRateHelper> sw5y = boost::make_shared<SwapRateHelper>(
        0.02, 5 * Years, WeekendsOnly(), Annual, ModifiedFollowing, Thirty360(),
        euribor6m);

    std::vector<boost::shared_ptr<RateHelper> > isdaRateHelper;

    isdaRateHelper.push_back(dp1m);
    isdaRateHelper.push_back(dp2m);
    isdaRateHelper.push_back(dp3m);
    isdaRateHelper.push_back(dp6m);
    isdaRateHelper.push_back(dp9m);
    isdaRateHelper.push_back(dp12m);
    isdaRateHelper.push_back(sw2y);
    isdaRateHelper.push_back(sw3y);
    isdaRateHelper.push_back(sw4y);
    isdaRateHelper.push_back(sw5y);

    // set up ISDA credit helper

    Handle<YieldTermStructure> emptyHandle;

    // boost::shared_ptr<CdsHelper> cds5y(new SpreadCdsHelper(
    //     0.03, 5 * Years, 1, WeekendsOnly(), Quarterly, Following,
    //     DateGeneration::CDS, Actual360(), 0.4, emptyHandle, true, true,
    //     Actual360(true), true, false));

    // cds5y->setIsdaEngineParameters(IsdaCdsEngine::Taylor, IsdaCdsEngine::None,
    //                                IsdaCdsEngine::Piecewise);

    std::vector<boost::shared_ptr<DefaultProbabilityHelper> > isdaCdsHelper;

    Handle<YieldTermStructure> rateTs(
        boost::make_shared<PiecewiseYieldCurve<Discount, LogLinear> >(
            0, WeekendsOnly(), isdaRateHelper, Actual365Fixed()));
    rateTs->enableExtrapolation();

    boost::shared_ptr<CdsHelper> cds5y(new SpreadCdsHelper(
        0.03, 5 * Years, 1, WeekendsOnly(), Quarterly, Following,
        DateGeneration::CDS, Actual360(), 0.4, rateTs, true, true,
        Actual360(true), true, false));

    isdaCdsHelper.push_back(cds5y);

    Handle<DefaultProbabilityTermStructure> defaultTs(boost::make_shared<
        PiecewiseDefaultCurve<SurvivalProbability, LogLinear> >(
        0, WeekendsOnly(), isdaCdsHelper, Actual365Fixed()));

    defaultTs->enableExtrapolation(); // why is this necessary ???

    // set up sample CDS trade

    boost::shared_ptr<CreditDefaultSwap> trade =
        MakeCreditDefaultSwap(5 * Years, 0.03);

    // set up isda engine

    // boost::shared_ptr<IsdaCdsEngine> isdaPricer =
    //     boost::make_shared<IsdaCdsEngine>(
    //         isdaCdsHelper, 0.4, isdaRateHelper);
    boost::shared_ptr<IsdaCdsEngine> isdaPricer =
        boost::make_shared<IsdaCdsEngine>(defaultTs,0.40,rateTs);

    // check the curves built by the engine

    // Handle<YieldTermStructure> isdaYts = isdaPricer->isdaRateCurve();
    // Handle<DefaultProbabilityTermStructure> isdaCts = isdaPricer->isdaCreditCurve();

    // std::cout << "isda rate 1m " << dp1m->latestDate() << " "
    //           << isdaYts->zeroRate(dp1m->latestDate(), Actual365Fixed(),
    //                                   Continuous) << std::endl;
    // std::cout << "isda rate 3m " << dp3m->latestDate() << " "
    //           << isdaYts->zeroRate(dp3m->latestDate(), Actual365Fixed(),
    //                                   Continuous) << std::endl;
    // std::cout << "isda rate 6m " << dp6m->latestDate() << " "
    //           << isdaYts->zeroRate(dp6m->latestDate(), Actual365Fixed(),
    //                                   Continuous) << std::endl;

    // std::cout << "isda hazard 5y " << cds5y->latestDate() << " "
    //           << isdaCts->hazardRate(cds5y->latestDate()) << std::endl;

    // price the trade

    trade->setPricingEngine(isdaPricer);

    Real npv = trade->NPV();

    std::cout << "Pricing of example trade with ISDA engine:" << std::endl;
    std::cout << "NPV = " << npv << std::endl;

}

void example03() {
    
    // this is the example from Apdx E in pricing and risk management of CDS, OpenGamma

    Date tradeDate(13,June,2011);

    Settings::instance().evaluationDate() = tradeDate;

    boost::shared_ptr<DepositRateHelper> dp1m =
        boost::make_shared<DepositRateHelper>(0.00445, 1 * Months, 2,
                                              WeekendsOnly(), ModifiedFollowing,
                                              false, Actual360());
    boost::shared_ptr<DepositRateHelper> dp2m =
        boost::make_shared<DepositRateHelper>(0.00949, 2 * Months, 2,
                                              WeekendsOnly(), ModifiedFollowing,
                                              false, Actual360());
    boost::shared_ptr<DepositRateHelper> dp3m =
        boost::make_shared<DepositRateHelper>(0.01234, 3 * Months, 2,
                                              WeekendsOnly(), ModifiedFollowing,
                                              false, Actual360());
    boost::shared_ptr<DepositRateHelper> dp6m =
        boost::make_shared<DepositRateHelper>(0.01776, 6 * Months, 2,
                                              WeekendsOnly(), ModifiedFollowing,
                                              false, Actual360());
    boost::shared_ptr<DepositRateHelper> dp9m =
        boost::make_shared<DepositRateHelper>(0.01935, 9 * Months, 2,
                                              WeekendsOnly(), ModifiedFollowing,
                                              false, Actual360());
    boost::shared_ptr<DepositRateHelper> dp1y =
        boost::make_shared<DepositRateHelper>(0.02084, 12 * Months, 2,
                                              WeekendsOnly(), ModifiedFollowing,
                                              false, Actual360());

    // this index is probably not important since we are not using
    // QL_INDEXED_COUPON - define it "isda compliant" anyway
    boost::shared_ptr<IborIndex> euribor6m = boost::make_shared<IborIndex>(
        "IsdaIbor", 6 * Months, 2, EURCurrency(), WeekendsOnly(),
        ModifiedFollowing, false, Actual360());

    boost::shared_ptr<SwapRateHelper> sw2y = boost::make_shared<SwapRateHelper>(
        0.01652, 2 * Years, WeekendsOnly(), Annual, ModifiedFollowing, Thirty360(),
        euribor6m);
    boost::shared_ptr<SwapRateHelper> sw3y = boost::make_shared<SwapRateHelper>(
        0.02018, 3 * Years, WeekendsOnly(), Annual, ModifiedFollowing, Thirty360(),
        euribor6m);
    boost::shared_ptr<SwapRateHelper> sw4y = boost::make_shared<SwapRateHelper>(
        0.02303, 4 * Years, WeekendsOnly(), Annual, ModifiedFollowing, Thirty360(),
        euribor6m);
    boost::shared_ptr<SwapRateHelper> sw5y = boost::make_shared<SwapRateHelper>(
        0.02525, 5 * Years, WeekendsOnly(), Annual, ModifiedFollowing, Thirty360(),
        euribor6m);
    boost::shared_ptr<SwapRateHelper> sw6y = boost::make_shared<SwapRateHelper>(
        0.02696, 6 * Years, WeekendsOnly(), Annual, ModifiedFollowing, Thirty360(),
        euribor6m);
    boost::shared_ptr<SwapRateHelper> sw7y = boost::make_shared<SwapRateHelper>(
        0.02825, 7 * Years, WeekendsOnly(), Annual, ModifiedFollowing, Thirty360(),
        euribor6m);
    boost::shared_ptr<SwapRateHelper> sw8y = boost::make_shared<SwapRateHelper>(
        0.02931, 8 * Years, WeekendsOnly(), Annual, ModifiedFollowing, Thirty360(),
        euribor6m);
    boost::shared_ptr<SwapRateHelper> sw9y = boost::make_shared<SwapRateHelper>(
        0.03017, 9 * Years, WeekendsOnly(), Annual, ModifiedFollowing, Thirty360(),
        euribor6m);
    boost::shared_ptr<SwapRateHelper> sw10y = boost::make_shared<SwapRateHelper>(
        0.03092, 10 * Years, WeekendsOnly(), Annual, ModifiedFollowing, Thirty360(),
        euribor6m);
    boost::shared_ptr<SwapRateHelper> sw11y = boost::make_shared<SwapRateHelper>(
        0.03160, 11 * Years, WeekendsOnly(), Annual, ModifiedFollowing, Thirty360(),
        euribor6m);
    boost::shared_ptr<SwapRateHelper> sw12y = boost::make_shared<SwapRateHelper>(
        0.03231, 12 * Years, WeekendsOnly(), Annual, ModifiedFollowing, Thirty360(),
        euribor6m);
    boost::shared_ptr<SwapRateHelper> sw15y = boost::make_shared<SwapRateHelper>(
        0.03367, 15 * Years, WeekendsOnly(), Annual, ModifiedFollowing, Thirty360(),
        euribor6m);
    boost::shared_ptr<SwapRateHelper> sw20y = boost::make_shared<SwapRateHelper>(
        0.03419, 20 * Years, WeekendsOnly(), Annual, ModifiedFollowing, Thirty360(),
        euribor6m);
    boost::shared_ptr<SwapRateHelper> sw25y = boost::make_shared<SwapRateHelper>(
        0.03411, 25 * Years, WeekendsOnly(), Annual, ModifiedFollowing, Thirty360(),
        euribor6m);
    boost::shared_ptr<SwapRateHelper> sw30y = boost::make_shared<SwapRateHelper>(
        0.03412, 30 * Years, WeekendsOnly(), Annual, ModifiedFollowing, Thirty360(),
        euribor6m);

    std::vector<boost::shared_ptr<RateHelper> > isdaYieldHelpers;
    
    isdaYieldHelpers.push_back(dp1m);
    isdaYieldHelpers.push_back(dp2m);
    isdaYieldHelpers.push_back(dp3m);
    isdaYieldHelpers.push_back(dp6m);
    isdaYieldHelpers.push_back(dp9m);
    isdaYieldHelpers.push_back(dp1y);
    isdaYieldHelpers.push_back(sw2y);
    isdaYieldHelpers.push_back(sw3y);
    isdaYieldHelpers.push_back(sw4y);
    isdaYieldHelpers.push_back(sw5y);
    isdaYieldHelpers.push_back(sw6y);
    isdaYieldHelpers.push_back(sw7y);
    isdaYieldHelpers.push_back(sw8y);
    isdaYieldHelpers.push_back(sw9y);
    isdaYieldHelpers.push_back(sw10y);
    isdaYieldHelpers.push_back(sw11y);
    isdaYieldHelpers.push_back(sw12y);
    isdaYieldHelpers.push_back(sw15y);
    isdaYieldHelpers.push_back(sw20y);
    isdaYieldHelpers.push_back(sw25y);
    isdaYieldHelpers.push_back(sw30y);

    Handle<YieldTermStructure> emptyHandle;

    bool useIsda = true;
    boost::shared_ptr<CdsHelper> cds6m(new SpreadCdsHelper(
        0.007927, 6 * Months, 1, WeekendsOnly(), Quarterly, Following,
        DateGeneration::CDS, Actual360(), 0.4, emptyHandle, true, true,
        Actual360(true), true, useIsda));
    boost::shared_ptr<CdsHelper> cds1y(new SpreadCdsHelper(
        0.007927, 1 * Years, 1, WeekendsOnly(), Quarterly, Following,
        DateGeneration::CDS, Actual360(), 0.4, emptyHandle, true, true,
        Actual360(true), true, useIsda));
    boost::shared_ptr<CdsHelper> cds3y(new SpreadCdsHelper(
        0.012239, 3 * Years, 1, WeekendsOnly(), Quarterly, Following,
        DateGeneration::CDS, Actual360(), 0.4, emptyHandle, true, true,
        Actual360(true), true, useIsda));
    boost::shared_ptr<CdsHelper> cds5y(new SpreadCdsHelper(
        0.016979, 5 * Years, 1, WeekendsOnly(), Quarterly, Following,
        DateGeneration::CDS, Actual360(), 0.4, emptyHandle, true, true,
        Actual360(true), true, useIsda));
    boost::shared_ptr<CdsHelper> cds7y(new SpreadCdsHelper(
        0.019271, 7 * Years, 1, WeekendsOnly(), Quarterly, Following,
        DateGeneration::CDS, Actual360(), 0.4, emptyHandle, true, true,
        Actual360(true), true, useIsda));
    boost::shared_ptr<CdsHelper> cds10y(new SpreadCdsHelper(
        0.020860, 10 * Years, 1, WeekendsOnly(), Quarterly, Following,
        DateGeneration::CDS, Actual360(), 0.4, emptyHandle, true, true,
        Actual360(true), true, useIsda));

    std::vector<boost::shared_ptr<DefaultProbabilityHelper> > isdaCdsHelpers;

    isdaCdsHelpers.push_back(cds6m);
    isdaCdsHelpers.push_back(cds1y);
    isdaCdsHelpers.push_back(cds3y);
    isdaCdsHelpers.push_back(cds5y);
    isdaCdsHelpers.push_back(cds7y);
    isdaCdsHelpers.push_back(cds10y);

    // set isda parameters
    for (Size i = 0; i < isdaCdsHelpers.size(); i++) {
        boost::dynamic_pointer_cast<CdsHelper>(isdaCdsHelpers[i])
            ->setIsdaEngineParameters(IsdaCdsEngine::Taylor, // None, Taylor
                                      IsdaCdsEngine::NoBias, // HalfDayBias, NoBias
                                      IsdaCdsEngine::Piecewise); // Piecewise // Flat
    }

    // set up isda engine

    boost::shared_ptr<IsdaCdsEngine> isdaPricer =
        boost::make_shared<IsdaCdsEngine>(
            isdaCdsHelpers, 0.4, isdaYieldHelpers);

    // check the curves built by the engine

    Handle<YieldTermStructure> isdaYts = isdaPricer->isdaRateCurve();
    Handle<DefaultProbabilityTermStructure> isdaCts = isdaPricer->isdaCreditCurve();

    std::cout << "Isda yield curve:" << std::endl;
    std::cout << "date;time;zeroyield" << std::endl;
    for (Size i = 0; i < isdaYieldHelpers.size(); i++) {
        Date d = isdaYieldHelpers[i]->latestDate();
        Real t = isdaYts->timeFromReference(d);
        std::cout << d << ";" << t << ";"
                  << isdaYts->zeroRate(d, Actual365Fixed(), Continuous).rate()
                  << std::endl;
    }

    std::cout << "Isda credit curve:" << std::endl;
    std::cout << "date;time;survivalprob" << std::endl;
    for (Size i = 0; i < isdaCdsHelpers.size(); i++) {
        Date d = isdaCdsHelpers[i]->latestDate();
        Real t = isdaCts->timeFromReference(d);
        std::cout << d << ";" << t << ";" << isdaCts->survivalProbability(d)
                  << std::endl;
    }
}


int main(int argc, char *argv[]) {

    try {
        Size example = 0;
        if (argc == 2)
            example = std::atoi(argv[1]);

        if (example == 0 || example == 1) {
            std::cout << "***** Running example #1 *****" << std::endl;
            example01();
        }

        if (example == 0 || example == 2) {
            std::cout << "***** Running example #2 *****" << std::endl;
            example02();
        }

        if (example == 0 || example == 3) {
            std::cout << "***** Running example #3 *****" << std::endl;
            example03();
        }

        return 0;
    }
    catch (exception &e) {
        cerr << e.what() << endl;
        return 1;
    }
    catch (...) {
        cerr << "unknown error" << endl;
        return 1;
    }
}
