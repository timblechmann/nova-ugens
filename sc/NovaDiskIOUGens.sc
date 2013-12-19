NovaDiskOut : UGen {
	*ar { arg numberOfChannels, filename;
		var args = numberOfChannels ++ [filename.size] ++ filename.asString.collectAs(_.ascii, Array);
		^this.multiNew('audio', *args)
	}
}

NovaDiskIn : UGen {
	classvar readerIDAllocator;

	*initClass {
		readerIDAllocator = StackNumberAllocator(0, 1073741824)
	}

	*cueSoundfile {|filename, completionMessage, server|
		var cueID = readerIDAllocator.alloc;

		if (server.isNil) {
			server = Server.default
		};

		server.sendMsg(\cmd, \NovaDiskIn, 0 /* open */, cueID, "filename", completionMessage);
		^cueID
	}

	*close {|cueID, completionMessage, server|
		if (server.isNil) {
			server = Server.default
		};

		server.sendMsg(\cmd, \NovaDiskIn, 2 /* close */, cueID, "", completionMessage);
	}

	*ar { arg numberOfChannels, cueID;
		^this.multiNew('audio', numberOfChannels, cueID)
	}
}
