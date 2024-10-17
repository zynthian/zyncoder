

#include <stdint.h>
typedef uint8_t bool;

#include <MCP4728.h>

#define MCP4728_I2C_ADDR 0x60
#define MCP4728_LDAC_GPIO 24

int main() {
    struct chip* mcp4728_chip = mcp4728_initialize(2, 3, MCP4728_LDAC_GPIO, MCP4728_I2C_ADDR);
    mcp4728_setaddress(mcp4728_chip, 0x64);
    mcp4728_deinitialize(mcp4728_chip);
    return 0;
}
