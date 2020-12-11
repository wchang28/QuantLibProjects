// HWCalibrate.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <ql/quantlib.hpp>

#ifdef BOOST_MSVC
/* Uncomment the following lines to unmask floating-point
exceptions. Warning: unpredictable results can arise...

See http://www.wilmott.com/messageview.cfm?catid=10&threadid=9481
Is there anyone with a definitive word about this?
*/
// #include <float.h>
// namespace { unsigned int u = _controlfp(_EM_INEXACT, _MCW_EM); }
#endif

#include <boost/timer/timer.hpp>
#include <iostream>
#include <vector>

using namespace QuantLib;

#if defined(QL_ENABLE_SESSIONS)
namespace QuantLib {

	Integer sessionId() { return 0; }

}
#endif

// simple rounding function
inline double round(double val, int decimal_places) {
    const double multiplier = std::pow(10.0, decimal_places);
    val *= multiplier;
    if (val < 0) return ceil(val - 0.5);
    val = floor(val + 0.5);
    return val/multiplier;
}

// hard - coded data
static std::vector<double> CreateSwaptionVolatilityList() {
    std::vector<double> vol;
    vol.push_back(0.1148);
    vol.push_back(0.1108);
    vol.push_back(0.1070);
    vol.push_back(0.1021);
    vol.push_back(0.1000);
    return vol;
}

class ModelCalibrator {
protected:
    const EndCriteria& endCriteria;
    std::vector<boost::shared_ptr<BlackCalibrationHelper>> helpers;
public:
    ModelCalibrator(const EndCriteria& _endCriteria): endCriteria(_endCriteria) {
    }
    void AddCalibrationHelper(const boost::shared_ptr<BlackCalibrationHelper>& helper) {
        helpers.push_back(helper);
    }
    void Calibrate(const boost::shared_ptr<HullWhite>& model, const boost::shared_ptr<PricingEngine>& engine, const Handle<YieldTermStructure>& curve, const std::vector<bool>& fixParameters) const {
        // assign pricing engine to all calibration helpers
        for (auto it = helpers.begin(); it != helpers.end(); it++) {
            const boost::shared_ptr<BlackCalibrationHelper>& helper = *it;
            helper->setPricingEngine(engine);
        }
        LevenbergMarquardt method;

        std::vector<boost::shared_ptr<CalibrationHelper>> tmp(helpers.size());
        for (Size i = 0; i < helpers.size(); ++i)
            tmp[i] = ext::static_pointer_cast<CalibrationHelper>(helpers[i]);

        if (fixParameters.empty()) {
            model->calibrate(tmp, method, endCriteria);
        } else {
            const std::vector<Real> weights;
            model->calibrate(tmp, method, endCriteria, NoConstraint(), weights, fixParameters);
        }
    }
};

int main()
{
    // general parameters
    Date tradeDate(15, February, 2002);
    Settings::instance().evaluationDate() = tradeDate;
    Date settlementDate(19, February, 2002);
    Calendar calendar = TARGET();
    DayCounter dayCounter = Actual360();

    // create market data: term structure and diagonal volatilities
    Handle<YieldTermStructure> curveHandle(boost::shared_ptr<YieldTermStructure>(new FlatForward(settlementDate, 0.04875825, Actual365Fixed())));
    auto vol = CreateSwaptionVolatilityList();

    // # create calibrator object
    EndCriteria endCriteria(10000, 100, 0.000001, 0.00000001, 0.00000001);
    ModelCalibrator calibrator(endCriteria);

    // add swaption helpers to calibrator
    for (size_t i = 0; i < vol.size(); i++) {
        auto t = i + 1;
        auto tenor = vol.size() - i;
        Handle<Quote> quoteHandle(boost::shared_ptr<Quote>(new SimpleQuote(vol[i])));
        boost::shared_ptr<SwaptionHelper> helper(new SwaptionHelper(
            Period(t, Years),
            Period(tenor, Years),
            quoteHandle,
            boost::shared_ptr<IborIndex>(new USDLibor(Period(3, Months), curveHandle)),
            Period(1, Years),
            dayCounter,
            dayCounter,
            curveHandle
        ));
        calibrator.AddCalibrationHelper(helper);
    }

    // create modeland pricing engine, calibrate modeland print calibrated parameters
    {
        std::cout << "case 1 : calibrate all involved parameters (HW1F : reversion, sigma)" << std::endl;
        boost::shared_ptr<HullWhite> model(new HullWhite(curveHandle));
        boost::shared_ptr<PricingEngine> engine(new JamshidianSwaptionEngine(model));
        std::vector<bool> fixedParameters;
        calibrator.Calibrate(model, engine, curveHandle, fixedParameters);
        std::cout << "calibrated reversion: " << round(model->params()[0], 5) << std::endl;
        std::cout << "calibrated sigma: " << round(model->params()[1], 5) << std::endl;
        std::cout << std::endl;
    }

    {
        std::cout << "case 2 : calibrate sigma and fix reversion to 0.05" << std::endl;
        boost::shared_ptr<HullWhite> model(new HullWhite(curveHandle, 0.05, 0.0001));
        boost::shared_ptr<PricingEngine> engine(new JamshidianSwaptionEngine(model));
        std::vector<bool> fixedParameters{ true, false };
        calibrator.Calibrate(model, engine, curveHandle, fixedParameters);
        std::cout << "fixed reversion: " << round(model->params()[0], 5) << std::endl;
        std::cout << "calibrated sigma: " << round(model->params()[1], 5) << std::endl;
        std::cout << std::endl;
    }

    {
        std::cout << "case 3 : calibrate reversion and fix sigma to 0.01" << std::endl;
        boost::shared_ptr<HullWhite> model(new HullWhite(curveHandle, 0.05, 0.01));
        boost::shared_ptr<PricingEngine> engine(new JamshidianSwaptionEngine(model));
        std::vector<bool> fixedParameters{ false, true };
        calibrator.Calibrate(model, engine, curveHandle, fixedParameters);
        std::cout << "calibrated reversion: " << round(model->params()[0], 5) << std::endl;
        std::cout << "fixed sigma: " << round(model->params()[1], 5) << std::endl;
        std::cout << std::endl;
    }
}