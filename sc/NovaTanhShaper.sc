NovaTanhShaper : UGen {
    *ar { arg sig, pregain = 1.0, mul = 1.0, add = 0.0;
        ^this.multiNew('audio', sig, pregain).madd(mul, add)
    }
}

NovaFastTanhShaper : UGen {
    *ar { arg sig, pregain = 1.0, mul = 1.0, add = 0.0;
        ^this.multiNew('audio', sig, pregain).madd(mul, add)
    }
}
