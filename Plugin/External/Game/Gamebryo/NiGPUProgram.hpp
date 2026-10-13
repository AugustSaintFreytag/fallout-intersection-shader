#pragma once

#include "NiRefObject.hpp"

class NiGPUProgram : public NiRefObject {
public:
	NiGPUProgram();
	virtual ~NiGPUProgram();

	enum ProgramType : uint32_t {
		PROGRAM_VERTEX = 0,
		PROGRAM_PIXEL  = 2,
		PROGRAM_GEOMETRY,
		PROGRAM_MAX
	};

	union {
		ProgramType		m_eProgramType;

		struct {
			uint8_t		ucProgramType;
			uint8_t		empty[2];
			bool		bEnabled;
		}; // NVR
	};
};

ASSERT_SIZE(NiGPUProgram, 0xC);