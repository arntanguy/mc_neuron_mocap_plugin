#include "../include/mc_neuron_mocap_plugin/MoCap.h"

MoCap_Data::MoCap_Data()
{
  m_Datas = Eigen::VectorXd::Zero(n_elements_);
  Datas = Eigen::MatrixXd::Zero(sequence_size, n_elements_);

  LeftHandPose_seq = Eigen::MatrixXd::Zero(sequence_size, 3);
  LeftHandAcc_seq = Eigen::MatrixXd::Zero(sequence_size, 3);
}

void MoCap_Data::convert_data(std::string & data)
{


  std::chrono::high_resolution_clock::time_point t_clock = std::chrono::high_resolution_clock::now();
  std::vector<double> double_data;
  std::stringstream data_stream(data);
  std::string double_val;
  while(std::getline(data_stream, double_val, ' ') )
  {

    double val;
    if (double_data.size() == n_elements_)
    {
      break;
    }
    try
    {
      val = stod(double_val);
      // std::cout << val << std::endl;
    }
    catch(const std::exception & e)
    {
      std::cout << "error at indx :" << static_cast<int>(double_data.size()) - 1  << " n_elements " << n_elements_ << " with val:" << double_val << '\n';
      std::cout << data.size() << std::endl;
      if ( double_data.size() - 1 < m_Datas.size())
      {
        val = m_Datas(double_data.size() - 1);
      }
      
    }
    if(double_data.size() < n_elements_)
    {
      double_data.push_back(val);
    };

  }

  m_Datas = Eigen::Map<Eigen::VectorXd, Eigen::Unaligned>(double_data.data(), double_data.size());
  FootState.x() = MoCap_Coord(RightHandThumb1, MoCap_Position).x();
  FootState.y() = MoCap_Coord(RightHandThumb1, MoCap_Position).y();
  Datas.block(0, 0, sequence_size - 1, Datas.cols()) = Datas.block(1, 0, sequence_size - 1, Datas.cols());
  Datas.row(sequence_size - 1) = m_Datas.segment(0, Datas.cols());

  std::chrono::duration<double, std::milli> time_span = std::chrono::high_resolution_clock::now() - t_clock;
  // mc_rtc::log::info(time_span.count());
}

sva::PTransformd MoCap_Data::get_pose(MoCap_Body_part part)
{
  return sva::PTransformd(MoCap_Quat(part).normalized().toRotationMatrix().transpose(),
                          MoCap_Coord(part, MoCap_Position));
}
sva::MotionVecd MoCap_Data::get_vel(MoCap_Body_part part)
{
  return sva::MotionVecd(MoCap_Coord(part, MoCap_Gyro), MoCap_Coord(part, MoCap_Velocity));
}
Eigen::Vector3d MoCap_Data::get_linear_acc(MoCap_Body_part part)
{
  return Eigen::Vector3d(MoCap_Coord(part, MoCap_Accelerated_Velocity));
}

Eigen::MatrixXd MoCap_Data::get_sequence(MoCap_Body_part part, MoCap_Parameters param, int size, int freq)
{

  // freq = mc_filter::utils::clampAndWarn(freq,0,data_freq_,"[mocap_plugin]");
  freq = std::min(data_freq_, std::max(0, freq));
  double f = static_cast<double>(freq);
  double data_f = static_cast<double>(data_freq_);
  int r = static_cast<int>(data_f / f);

  size = std::min(sequence_size / r, std::max(0, size));

  Eigen::MatrixXd seq = Eigen::MatrixXd::Zero(sequence_size, 3);

  if(param > MoCap_Quaternion)
  {
    seq = Datas.block(sequence_size - size, 16 * part + param * 3 + 1, size, 3);
  }
  else if(param == MoCap_Quaternion)
  {

    seq = Eigen::MatrixXd::Zero(sequence_size, 4);
    seq = Datas.block(sequence_size - size, 16 * part + param * 3 + 1, size, 4);
  }
  else
  {

    seq = Datas.block(sequence_size - size, 16 * part + param * 3, size, 3);
  }

  if(param == MoCap_Accelerated_Velocity)
  {

    seq.col(2) -= Eigen::VectorXd::Ones(seq.rows());
    seq *= 9.8;
  }
  Eigen::MatrixXd Output(Eigen::MatrixXd::Zero(sequence_size / r, seq.cols()));
  int incr = 2;
  for(int i = 0; i < Output.rows(); i++)
  {

    Output.block(Output.rows() - 1 - i, 0, 1, Output.cols()) = seq.block(seq.rows() - 1 - i * r, 0, 1, seq.cols());
  }

  return Output.transpose();
}

Eigen::VectorXd MoCap_Data::GetParameters(MoCap_Body_part part, MoCap_Parameters param)
{

  // std::cout << "required part" << part << std::endl;
  if((part > RightHandThumb1 || part < 0) && param > MoCap_Position)
  {
    mc_rtc::log::warning("[mocap plugin] required index out of bounds");
    if(param == MoCap_Quaternion)
    {
      return Eigen::VectorXd::Zero(4);
    }
    return Eigen::VectorXd::Zero(3);
  }
  if(param > MoCap_Quaternion)
  {

    return m_Datas.segment(16 * part + param * 3 + 1, 3);
  }
  else if(param == MoCap_Quaternion)
  {

    return m_Datas.segment(16 * part + param * 3, 4);
  }
  else
  {
    return m_Datas.segment(16 * part + param * 3, 3);
  }
}

Eigen::Vector3d MoCap_Data::MoCap_Coord(MoCap_Body_part joint, MoCap_Parameters param)
{

  Eigen::Vector3d DataOut(GetParameters(joint, param));
  if(param == MoCap_Accelerated_Velocity)
  {

    DataOut.z() -= 1;
    DataOut *= -9.8;
  }

  return DataOut;
}

Eigen::Quaterniond MoCap_Data::MoCap_Quat(MoCap_Body_part joint)
{
  Eigen::VectorXd vec(GetParameters(joint, MoCap_Quaternion));
  Eigen::Quaterniond output;

  output.x() = vec[1];
  output.y() = vec[2];
  output.z() = vec[3];
  output.w() = vec[0];

  return output;
}
