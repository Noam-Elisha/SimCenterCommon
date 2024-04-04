
// Written fmk 04/24

#include <QApplication>
//#include <QProcess>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <RunPythonInThread.h>
#include <Utils/ProgramOutputDialog.h>
#include <SimCenterPreferences.h>

//
// RunPythonInThread Class first .. easiest!
//

RunPythonInThread::RunPythonInThread(const QString &theScript, const QStringList &theArgs, const QString &theWorkDir) {
  script = theScript;
  args = theArgs;
  workDir = theWorkDir;
}

void
RunPythonInThread::runProcess(void) {
  
  RunPython *worker = new RunPython(script, args, workDir);
  worker->moveToThread(&workerThread);

  //
  // set up connections between thread and worker and thread/worker and this
  //
  
  // connections between the worker thread and the working in it
  connect(&workerThread, &QThread::started, worker, &RunPython::processScript);
  connect(&workerThread, &QThread::finished, worker, &QObject::deleteLater);

  // connections between the worker running in the thread and this, the object that created both
  connect(worker, &RunPython::processFinished, this, &RunPythonInThread::handleFinished);
  connect(worker, &RunPython::errorMessage, this, &RunPythonInThread::errorMessage);
  connect(worker, &RunPython::statusMessage, this, &RunPythonInThread::statusMessage);

  //
  // with connections all set .. start the thread .. when done it shoulud cause chain reaction that deletes everything!!
  //
  
  workerThread.start();
}

RunPythonInThread::~RunPythonInThread() {
  
  workerThread.quit();
  workerThread.wait();
  qDebug() << "RunPythonInThread::~RunPythonInThread()"; // just checking its called .. to make sure proper shutdown
}

void
RunPythonInThread::handleFinished(int exitCode) {
  qDebug() << "RunPythonInThread::handleFinished exitCode: " << exitCode;
  emit processFinished(exitCode);

  // this will delete when no events left in queue
  this->deleteLater();
}

void
RunPythonInThread::errorMessage(const QString &message) {

  ProgramOutputDialog *progressDialog = ProgramOutputDialog::getInstance();  
  if(message.isEmpty())
    return;
  
  progressDialog->appendErrorMessage(message);
}

void
RunPythonInThread::statusMessage(const QString &message) {

  ProgramOutputDialog *progressDialog = ProgramOutputDialog::getInstance();  
  if(message.isEmpty())
    return;
  
  progressDialog->appendText(message);

}


//
// and now the RunPython that runs the python script in the new thread using QProcess
//

RunPython::RunPython(const QString &theScript, const QStringList &theArgs, const QString &theWorkDir)
{
  script = theScript;
  args = theArgs;
  workDir = theWorkDir;
}

RunPython::~RunPython()
{
  qDebug() << "RunPython::~RunPython";
}


void
RunPython::processScript() {

  //
  // check if script, dir & python exe exists, if not signal shutdown by emitting processFinished
  //

  QDir workDirectory(workDir);
  if (!workDirectory.exists()) {
    emit errorMessage(QString("ModularPythonInThread::ModularPythonInThread no existant workdir: ")+workDir);
    //pythonProcessDone("Process failed to launch due to non-existant working dir.");
    emit processFinished(-1);
    return;
  }

  QFile pythonScript(script);
  if (!pythonScript.exists()) {
    emit errorMessage(QString("ModularPythonInThread::ModularPythonInThread no script exists: ")+script);
    //pythonProcessDone("Process failed to launch due to missing script.");
    emit processFinished(-1);    
    return;
  }
  
  SimCenterPreferences *preferences = SimCenterPreferences::getInstance();
  QString python = preferences->getPython();
  
  QFileInfo pythonFile(python);
  if (!pythonFile.exists()) {
    emit errorMessage("NO VALID PYTHON - Read the Manual & Check your Preferences");
    //pythonProcessDone("Process failed to launch due to missing python exe.");
    emit processFinished(-1);
  }
  
  //
  // set up a QProcess in which to run our command
  //

  process = new QProcess(this);
  process->setWorkingDirectory(workDir);
  process->setProcessChannelMode(QProcess::MergedChannels);

  //
  // do connections to capture outputs and exit
  //
  
  connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &RunPython::handleProcessFinished);
  connect(process, &QProcess::readyReadStandardOutput, this, &RunPython::handleProcessTextOutput);
  connect(process, &QProcess::readyReadStandardError, this, &RunPython::handleProcessErrorOutput);

  //
  // run python script in the process
  //

  QStringList argsWithScript; argsWithScript << script << args;
  process->start(python, argsWithScript);  

  
  // emit errorMessage("RunPython script:" + script);
  //emit statusMessage("Hello World");
  // emit processFinished(0);
}

void RunPython::handleProcessTextOutput(void) {
    QByteArray output = process->readAllStandardOutput();
    qDebug() << "OUTPUT_OUTPUT: " << output;
    emit statusMessage(QString(output));
    // QApplication::processEvents();
}


void RunPython::handleProcessErrorOutput(void) {
    QByteArray output = process->readAllStandardError();

    emit errorMessage(QString(output));
    qDebug() << "ERROR_OUTPUT: " << output;
    //QApplication::processEvents();
}

void RunPython::handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {

  qDebug() << "RunPython::handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)" << exitCode << " " << exitStatus;
  
  if(exitStatus == QProcess::ExitStatus::CrashExit) {
    //QString errText("Error, the python process " + processName + " crashed");
    QString errText("Error, the python process crashed");    
    emit errorMessage(errText);
    //        this->getProgressDialog()->hideProgressBar();
    
    emit processFinished(-1);
    return;
  }
  
  if(exitCode != 0) {
    //QString errText("There was an error in the python process " + processName + ", the exit code is " + QString::number(exitCode));
    QString errText("There was an error in the python process, the exit code is " + QString::number(exitCode));    
    emit errorMessage(errText);
    // this->getProgressDialog()->hideProgressBar();
    
    emit processFinished(-1);
    return;
  }
  
  QApplication::processEvents();
  qDebug() << "RunPython::emitting processFinished";
  emit processFinished(0);
}

