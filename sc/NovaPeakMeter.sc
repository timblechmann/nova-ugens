PeakRMS : UGen {

    *kr { arg sig, replyRate = 20.0, levelLag = 3, cmdName = '/reply', replyID = -1;
        this.new1('control', sig.asArray, replyRate, levelLag, cmdName, replyID);
        ^0.0        // PeakRMS has no output
    }

    *ar { arg sig, replyRate = 20.0, levelLag = 3, cmdName = '/reply', replyID = -1;
        this.new1('audio', sig.asArray, replyRate, levelLag, cmdName, replyID);
        ^0.0        // PeakRMS has no output
    }

    *new1 { arg rate, sig, replyRate, levelLag, cmdName, replyID;
        var ascii = cmdName.ascii;
        var args = [rate, replyRate, levelLag, replyID, sig.size]
            .addAll(sig.flatten)
            .add(ascii.size).addAll(ascii);
        ^super.new1(*args);
    }

    numOutputs { ^0 }
    writeOutputSpecs {}
}