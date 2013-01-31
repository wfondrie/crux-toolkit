/*******************************************************************************
 Copyright 2006-2009 Lukas Käll <lukas.kall@cbr.su.se>

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.

 *******************************************************************************/
#include<iterator>
#include<vector>
#include<algorithm>
#include<numeric>
#include<functional>
#include<fstream>
#include<sstream>
using namespace std;

#include "Globals.h"
#include "LogisticRegression.h"
#include "Numerical.h"
const double LogisticRegression::gRange = 35.0;

void invlogit(double& out, double in) {
  double e = exp(in);
  out = e / (1 + e);
}

double invlogit(double in) {
  double e = exp(in);
  return e / (1 + e);
}

double logit(double p) {
  return log(p / (1 - p));
}

void LogisticRegression::limitg() {
  for (int ix = gnew.numberEntries(); ix--;) {
    gnew.packedReplace(ix, min(gRange, max(-gRange, gnew[ix])));
    assert(isfinite(gnew[ix]));
  }
}

void LogisticRegression::limitgamma() {
  for (int ix = gamma.numberEntries(); ix--;) {
    gamma.packedReplace(ix, min(gRange, max(-gRange, gamma[ix])));
    assert(isfinite(gamma[ix]));
  }
}

void LogisticRegression::calcPZW() {

  for (int ix = z.numberEntries(); ix--;) {
    assert(isfinite(g[ix]));
    double e = exp(g[ix]);
    assert(isfinite(e));
    Numerical num(1e-15);
    p[ix] = min(max(e / (1 + e), num.epsilon), 1
        - num.epsilon);
    assert(isfinite(p[ix]));
    w.packedReplace(ix, max(m[ix] * p[ix] * (1 - p[ix]), num.epsilon));
    assert(isfinite(w[ix]));
    z.packedReplace(ix, min(gRange, max(-gRange, g[ix] +
        (((double)y[ix]) - p[ix] * ((double)m[ix])) / w[ix])));
    assert(isfinite(z[ix]));
  }
}

void LogisticRegression::initg() {
  BaseSpline::initg();
  int n = x.size();
  p.resize(n);
  gnew = PackedVector(n);
  for (int ix = g.size(); ix--;) {
    double p = (y[ix] + 0.05) / (m[ix] + 0.1);
    gnew.packedReplace(ix, log(p / (1 - p)));
    assert(isfinite(p));
    assert(isfinite(g[ix]));
  }
  //#define OUTPUT_DEBUG_FILES
#undef OUTPUT_DEBUG_FILES
#ifdef OUTPUT_DEBUG_FILES
  ofstream drFile("decoyRate.bins", ios::out), xvalFile("xvals.bins", ios::out);
  ostream_iterator<double> xvalIt(xvalFile, "\n");
  copy(x.begin(), x.end(), xvalIt);
  for (size_t yix = 0; yix < y.size(); ++yix) {
    drFile << y[yix] / (double) m[yix] << endl;
  }
  drFile.close();
#endif
}
