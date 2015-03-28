NovaDiskOut : UGen {
	*ar { arg signal, filename;
		var arrayedSignal = signal.asArray;
		var args = [arrayedSignal.size] ++ arrayedSignal ++ [filename.size] ++ filename.asString.collectAs(_.ascii, Array);
		^this.multiNew('audio', *args)
	}
}

NovaDiskIn : MultiOutUGen {
	classvar readerIDAllocator;

	*initClass {
		readerIDAllocator = StackNumberAllocator(0, 1073741824)
	}

	*cueSoundfile {|filename, completionMessage, server|
		var cueID = readerIDAllocator.alloc;

		if (server.isNil) {
			server = Server.default
		};

		server.sendMsg(\cmd, \NovaDiskIn, 0 /* open */, cueID, filename, completionMessage);
		^cueID
	}

	*close {|cueID, completionMessage, server|
		if (server.isNil) {
			server = Server.default
		};

		server.sendMsg(\cmd, \NovaDiskIn, 2 /* close */, cueID, "", completionMessage);
		readerIDAllocator.free( cueID )
	}

	*closeAll {|completionMessage, server|
		if (server.isNil) {
			server = Server.default
		};

		server.sendMsg(\cmd, \NovaDiskIn, 3 /* closeAll */, 0, "", completionMessage);
		NovaDiskIn.initClass;
	}

	*ar { arg numberOfChannels, cueID;
		^this.multiNew('audio', numberOfChannels, cueID)
	}

	init { arg numberOfChannels, cueID;
		inputs = [numberOfChannels, cueID];
		^this.initOutputs(numberOfChannels, rate)
	}
}


+ Server {
	record {|path|
		Environment.use {

			// prepare path
			if (path.isNil) {
				if(File.exists(thisProcess.platform.recordingsDir).not) {
					thisProcess.platform.recordingsDir.mkdir
				};

				// temporary kludge to fix Date's brokenness on windows
				if(thisProcess.platform.name == \windows) {
					path = thisProcess.platform.recordingsDir +/+ "SC_" ++ Main.elapsedTime.round(0.01) ++ ".caf";

				} {
					path = thisProcess.platform.recordingsDir +/+ "SC_" ++ Date.localtime.stamp ++ ".caf";
				};
			};

			~channels = this.recChannels;

			"NovaServerRecorder: recording %".format(path).postln;

			recordNode = {
				var sig = In.ar(0, ~channels);
				NovaDiskOut.ar(sig, path);
				Silent.ar
			}.play(target: RootNode(this), outbus: 0, fadeTime: 0.0001, addAction: \addToTail);

			CmdPeriod.doOnce {
				this.stopRecording
			}
		}
	}

	stopRecording {
		if (recordNode.notNil) {
			recordNode.free;
			recordNode = nil
		}
	}

	pauseRecording {
		"Nova's  ServerRecorder does not support paused recordings".warn
	}
}
