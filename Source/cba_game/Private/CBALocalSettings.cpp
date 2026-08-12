#include "CBALocalSettings.h"

UCBALocalSettings* UCBALocalSettings::Get()
{
    return Cast<UCBALocalSettings>(UGameUserSettings::GetGameUserSettings());
}
