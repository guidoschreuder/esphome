#ifndef EBUS_SEND_COMMAND_H
#define EBUS_SEND_COMMAND_H

#include <stdint.h>

#include "TelegramBase.h"

namespace Ebus {

class SendCommand : public TelegramBase {
public:
  SendCommand();
  SendCommand(uint8_t QQ, uint8_t ZZ, uint8_t PB, uint8_t SB, uint8_t NN, uint8_t *data);
  bool canRetry(int8_t max_tries);
  uint8_t getCRC();

protected:
  uint8_t numTries = 0;

};

}  // namespace Ebus

#endif
