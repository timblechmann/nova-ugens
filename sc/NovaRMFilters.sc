NovaLowshelf : UGen {
    *ar { arg in, freq = 440.0, gain = 0.0, mul = 1.0, add = 0.0;
        ^this.multiNew('audio', in, freq, gain.dbamp, mul, add)
    }

    checkInputs { ^this.checkSameRateAsFirstInput }

}

NovaHighshelf : NovaLowshelf {
}

NovaEQ : UGen {
    *ar { arg in, freq = 440.0, q = 0.5, gain = 0.0, mul = 1.0, add = 0.0;
        ^this.multiNew('audio', in, freq, q, gain.dbamp, mul, add)
    }

    checkInputs { ^this.checkSameRateAsFirstInput }
}

NovaLPF : UGen {
    *ar { arg in, freq = 440.0, mul = 1.0, add = 0.0;
        ^this.multiNew('audio', in, freq, mul, add)
    }

    checkInputs { ^this.checkSameRateAsFirstInput }
}

NovaHPF : NovaLPF {
}

NovaBPF : UGen {
    *ar { arg in, freq = 440.0, bw = 10, mul = 1.0, add = 0.0;
        ^this.multiNew('audio', in, freq, mul, add)
    }

    checkInputs { ^this.checkSameRateAsFirstInput }
}


NovaBRF : NovaBPF {
}
