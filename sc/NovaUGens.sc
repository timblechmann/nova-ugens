GendyI : UGen {
    *ar { arg ampdist=1, durdist=1, adparam=1.0, ddparam=1.0, minfreq=440, maxfreq=660, ampscale= 0.5, durscale=0.5,
        initCPs= 12, knum, interpolation = 0, mul=1.0,add=0.0;
        ^this.multiNew('audio', ampdist, durdist, adparam, ddparam, minfreq, maxfreq, ampscale, durscale, initCPs, knum ? initCPs, interpolation).madd( mul, add )
    }

    *kr {arg ampdist=1, durdist=1, adparam=1.0, ddparam=1.0, minfreq=20, maxfreq=1000, ampscale= 0.5, durscale=0.5,
        initCPs = 12, knum, interpolation = 0, mul=1.0,add=0.0;
        ^this.multiNew('control', ampdist, durdist, adparam, ddparam, minfreq, maxfreq, ampscale, durscale, initCPs, knum ? initCPs, interpolation).madd( mul, add )
    }
}

FBAM : PureUGen {
    *ar { arg sig, feedback = 0.1, mul = 1.0, add = 0.0;
        ^this.multiNew('audio', sig, feedback).madd( mul, add )
    }
}

PulseDPW2 : PureUGen {
	*ar { arg freq = 220, width = 0.5, phase = 0.0, mul = 1.0, add = 0.0;
		^this.multiNew('audio', freq, width, phase.linlin(-1, 1, -pi, pi)).madd( mul, add )
	}

	*kr { arg freq = 220, width = 0.5, phase = 0.0, mul = 1.0, add = 0.0;
		^this.multiNew('control', freq, width, phase.linlin(-1, 1, -pi, pi)).madd( mul, add )
	}
}

NovaSinOsc : UGen {
	*ar { arg freq, phase = 0, interpolation = 1, mul=1.0, add=0.0;
		^this.multiNew('audio', freq, phase, interpolation).madd( mul, add )
	}
	*kr { arg freq, phase = 0, interpolation = 1, mul=1.0, add=0.0;
		^this.multiNew('audio', freq, phase, interpolation).madd( mul, add )
	}
}

NovaOsc : UGen {
	*ar { arg buf, freq, phase = 0, interpolation = 1, mul=1.0, add=0.0;
		^this.multiNew('audio', buf, freq, phase, interpolation).madd( mul, add )
	}
	*kr { arg buf, freq, phase = 0, interpolation = 1, mul=1.0, add=0.0;
		^this.multiNew('audio', buf, freq, phase, interpolation).madd( mul, add )
	}
}

ES3Interleaver_96k : MultiOutUGen {
	*ar {|s0, s1, s2, s3, s4, s5, s6, s7, mode = 0|
		var args = [s0, s1, s2, s3, s4, s5, s6, s7];

		args = args.collect {|sig|
			if (sig.rate != \audio) {
				DC.ar(sig)
			} {
				sig
			}
		};

		args = args ++ [mode];

		^this.multiNew('audio', *args);
	}

	init { arg s0, s1, s2, s3, s4, s5, s6, s7, mode;
		inputs = [s0, s1, s2, s3, s4, s5, s6, s7, mode];
		^this.initOutputs(4, rate)
	}
}
