bnn-hep
=======

A tool to enable Bayesian neural networks (BNN) for high-energy physics. The project is an adaptation of BNN from
[FBM package](http://www.cs.toronto.edu/~radford/fbm.software.html) developed by R.M. Neal, relevant fragments of which
are added to the repository. Key differences with respect to the original framework are the following:
 * Neural networks for problem of binary classification are only considered (no regression, only two classes of events
   are allowed)
 * Customisation is done in a convenient way with the help of a configuraion file. User runs a single executable
 * Events are described in [ROOT](http://root.cern.ch) files
 * Implementation can handle weighted events (though, only physical, sctrictly positive weights are allowed)
 * C++ code that defines a specific BNN is produced as the result of training

Configuration file is parsed with a pruned version of [libconfig](http://www.hyperrealm.com/libconfig/) library. The tool
uses [ROOT](http://root.cern.ch) and [Boost](http://www.boost.org) libraries.

Being a detached tool, bnn-hep cannot compete with a generalised MVA framework in the level of flexibility and convenience
of usage. In addition to it, development is complicated by the fact that original FBM framework is coded in pure C. For
this reason no active development of the tool is expected, but only bugfixes and minor improvements. In a longer time
scale I hope to reimplement BNN in [TMVA package](http://tmva.sourceforge.net/).
