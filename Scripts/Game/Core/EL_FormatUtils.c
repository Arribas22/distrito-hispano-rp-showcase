class EL_FormatUtils
{
	//------------------------------------------------------------------------------------------------
	static string AbbreviateNumber(int number)
	{
		string result;
		if (number >= 1000000000)
		{
			result = string.Format("%1B", (number / 1000000000.0).ToString(-1, 1));
		}
		else if (number >= 1000000)
		{
			result = string.Format("%1M", (number / 1000000.0).ToString(-1, 1));
		}
		else if (number >= 1000)
		{
			result = string.Format("%1K", (number / 1000.0).ToString(-1, 1));
		}
		else
		{
			result = number.ToString();
		}

		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! Format number with thousand separators
	//! \param number The number to format
	//! \return Formatted string (e.g., 15000 -> "15,000")
	static string DecimalSeperator(int number)
	{
		string numStr = number.ToString();
		string result = "";
		int len = numStr.Length();
		
		for (int i = 0; i < len; i++)
		{
			if (i > 0 && (len - i) % 3 == 0)
				result += ",";
			
			result += numStr.Get(i);
		}
		
		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! Format time in seconds to HH:MM:SS format
	//! \param seconds Time in seconds
	//! \return Formatted time string
	static string FormatTime(int seconds)
	{
		int hours = seconds / 3600;
		int minutes = (seconds % 3600) / 60;
		int secs = seconds % 60;
		
		return string.Format("%1:%2:%3", 
			hours.ToString(2), 
			minutes.ToString(2), 
			secs.ToString(2)
		);
	}

	//------------------------------------------------------------------------------------------------
	//! Format distance in meters to readable format
	//! \param meters Distance in meters
	//! \return Formatted distance string (e.g., "1.5 km" or "150 m")
	static string FormatDistance(float meters)
	{
		if (meters >= 1000)
			return string.Format("%.1f km", meters / 1000);
		
		return string.Format("%.0f m", meters);
	}
}
