var cpuDataSets = [];

$(function() {
  $("#status").text("");
  get_temp();
  get_cpu_use();
  setInterval("get_cpu_use()", 1000);
  setInterval("get_temp()", 5000);
});

function get_cpu_use()
{
	$.ajax({
		url: "cpu.api",
		type: "post",
		data: { counter:"0" }
	}).done(function(cpu_info)
	{
      $("#cpuinfo").text(cpu_info[0]+"% ("+(cpu_info.length-1)+" CPU)");       
      var needs_init = 0;
      
      if (cpuDataSets.length==0)
      {
         needs_init = 1;
         for (var n=cpu_info.length-1; n>=0; n--)
         {
            cpuDataSets.push(new TimeSeries());            
            if (n>0)
            {
               $("#cpu0").after("<br/><canvas id=\"cpu" + n +"\" width=\"500\" height=\"100\" />");
            }
   	   }
      }
	    
      for (var n=0; n<cpu_info.length; n++)
      {
         cpuDataSets[n].append(new Date().getTime(), cpu_info[n]);
         if (needs_init == 1)
         {
            initChart(n);
         }
      }
	});
}

function get_temp()
{
    $.ajax({
        url: "temp.api",
        type: "post",
    }).done(function(data)
    {
        $("#temp").text(data);
    });
}

function initChart(cpuId)
{
  var seriesOptions;
  if (cpuId==0)
  {
    seriesOptions =
    { 
      strokeStyle: 'rgba(255, 0, 0, 1)',
      fillStyle: 'rgba(255, 0, 0, 0.2)',
      lineWidth: 3
    };
  }
  else
  {
    seriesOptions =
    { 
     strokeStyle: 'rgba(0, 255, 0, 1)',
     fillStyle: 'rgba(0, 255, 0, 0.2)',
     lineWidth: 2
    };
  }
  var timeline = new SmoothieChart(
  {
     labels: {disabled: true},
     maxValue: 100,
     minValue: 0,
     millisPerPixel: 20,
     grid:
     {
        strokeStyle: '#555555',
        lineWidth: 1,
        millisPerLine: 1000,
        verticalSections: 4 
     }
  });
  timeline.addTimeSeries(cpuDataSets[cpuId], seriesOptions);
  timeline.streamTo(document.getElementById('cpu'+cpuId), 1000);
}
